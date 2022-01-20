#include <memory>
#include <hiredis/hiredis.h>
#include <string> // TODO: move this out of here, this is just because of defining string type specifier

template<typename T>
constexpr char* getTypeSpecifier() = delete;

template<>
constexpr char* getTypeSpecifier<std::string>() = delete;

template <typename KEY, typename VALUE>
class UnorderedRedis
{
    UnorderedRedis()
    {
        context = redisConnect("127.0.0.1",6379);
    }

    void insert(KEY key , VALUE value)
    {
        redisCommand(context.get(),"set")
    }

    private:
    std::unique_ptr<redisContext> context;
};