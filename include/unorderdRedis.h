#include <memory>
#include <hiredis/hiredis.h>
#include <string>
#include <cassert>

//#####################
template<unsigned...>struct seq{using type=seq;};
template<unsigned N, unsigned... Is>
struct gen_seq_x : gen_seq_x<N-1, N-1, Is...>{};
template<unsigned... Is>
struct gen_seq_x<0, Is...> : seq<Is...>{};
template<unsigned N>
using gen_seq=typename gen_seq_x<N>::type;

template<size_t S>
using size=std::integral_constant<size_t, S>;

template<class T, size_t N>
constexpr size<N> length( T const(&)[N] ) { return {}; }
template<class T, size_t N>
constexpr size<N> length( std::array<T, N> const& ) { return {}; }

template<class T>
using length_t = decltype(length(std::declval<T>()));

constexpr size_t string_size() { return 0; }
template<class...Ts>
constexpr size_t string_size( size_t i, Ts... ts ) {
  return (i?i-1:0) + string_size(ts...);
}
template<class...Ts>
using string_length=size< string_size( length_t<Ts>{}... )>;

template<class...Ts>
using combined_string = std::array<char, string_length<Ts...>{}+1>;

template<class Lhs, class Rhs, unsigned...I1, unsigned...I2>
constexpr const combined_string<Lhs,Rhs>
concat_impl( Lhs const& lhs, Rhs const& rhs, seq<I1...>, seq<I2...>)
{
    return {{ lhs[I1]..., rhs[I2]..., '\0' }};
}

template<class Lhs, class Rhs>
constexpr const combined_string<Lhs,Rhs>
concat(Lhs const& lhs, Rhs const& rhs)
{
    return concat_impl(lhs, rhs, gen_seq<string_length<Lhs>{}>{}, gen_seq<string_length<Rhs>{}>{});
}

template<class T0, class T1, class... Ts>
constexpr const combined_string<T0, T1, Ts...>
concat(T0 const&t0, T1 const&t1, Ts const&...ts)
{
    return concat(t0, concat(t1, ts...));
}
//#####################

template<typename T>
auto getValue(const redisReply* const reply)->T;

template<>
auto getValue<std::string>(const redisReply* const reply) ->std::string
{
    return std::string(reply->str);
}

template<>
auto getValue<int>(const redisReply* const reply) ->int
{
    return std::stoi(reply->str);
}

template<>
auto getValue<float>(const redisReply* const reply) ->float
{
    return std::stof(reply->str);
}

template<>
auto getValue<double>(const redisReply* const reply) ->double
{
    return std::stod(reply->str);
}


//#####################

///
/// \brief u_data get the suitable data to push in redis based on type
/// \param i
/// \return
///
const char* u_data(const std::string& i)
{
    return i.c_str();
}

inline int u_data(int i)
{
    return i;
}

inline float u_data(float i)
{
    return i;
}
inline double u_data(double i)
{
    return i;
}

template<typename T>
class TypeSpecifier;

template<>
class TypeSpecifier<std::string>
{
public:
    //static constexpr char v[]="\\\"%s\\\"";
    static constexpr char v[]="%s";
};
template<>
class TypeSpecifier<int>
{
public:
    static constexpr char v[]="%i";
};

template<>
class TypeSpecifier<float>
{
public:
    static constexpr char v[]="%f";
};

template<>
class TypeSpecifier<double>
{
public:
    static constexpr char v[]="%f";
};


//void close_redis(redisContext* rc)
//{
////    if (rc->fd > 0)
////           close(rc->fd);
////       if (rc->obuf != NULL)
////           sdsfree(c->obuf);
//       if (rc->reader != NULL)
//           redisReaderFree(rc->reader);
//       delete rc;
//}

template <typename KEY, typename VALUE>
class UnorderedRedis;


template<typename KEY>
bool operator==(const typename UnorderedRedis<KEY,std::string>::Reference& r,const std::string& s){
    return static_cast<std::string>(r) == s;
}

template <typename KEY, typename VALUE>
class UnorderedRedis
{
public:

    struct Reference
    {
        Reference(UnorderedRedis& unorderedRedis, KEY key):_redis(unorderedRedis),_key(key)
        {

        }

        void
        operator=(const VALUE& value)
        {
            _redis.insert(_key,value);
        }


        operator VALUE() const
        {
            return _redis.value(_key);
        }
        UnorderedRedis& _redis;
        KEY _key;
    };
    UnorderedRedis()
    {
        context = std::unique_ptr<redisContext>(redisConnect("127.0.0.1",6379));
    }

    ///
    /// \brief insert value to redis
    /// \param key
    /// \param value
    ///
    void insert(const KEY& key , const VALUE& value) noexcept
    {
        constexpr auto command = concat("set ",TypeSpecifier<KEY>::v," ",TypeSpecifier<VALUE>::v);
        redisCommand(context.get(),command.data(),u_data(key),u_data(value));
    }

    ///
    /// \brief contains, check if a value already contains in  redis
    /// \param key
    /// \return
    ///
    bool contains(const KEY& key)
    {
        constexpr auto command = concat("exists ",TypeSpecifier<KEY>::v);
        void * data = redisCommand(context.get(),command.data(),u_data(key));
        if(!data)
        {
            return false;
        }

        #ifndef NDEBUG
        assert ((static_cast<redisReply *>(data))->type == REDIS_REPLY_INTEGER);
        #endif
        return bool((static_cast<redisReply *>(data))->integer);
    }

    ///
    /// \brief operator [], get a lazy loaded value from redis
    /// \param key
    /// \return
    ///
    Reference operator[](const KEY& key)
    {
        return Reference(*this,key);
    }

    ///
    /// \brief get a value from redis
    /// \param key
    /// \return
    ///
    VALUE value(const KEY& key)
    {
        constexpr auto command = concat("get ",TypeSpecifier<KEY>::v);
        void * data = redisCommand(context.get(),command.data(),u_data(key));
        if(!data)
        {
            insert(key,VALUE{});
            return value(key);
        }

        #ifndef NDEBUG
        //assert ((static_cast<redisReply *>(data))->type == REDIS_REPLY_INTEGER); TODO: fix this
        #endif
        return getValue<VALUE>((static_cast<redisReply *>(data)));
    }

    private:
    std::unique_ptr<redisContext> context; // TODO: need to add deleter to close socket properly
};

