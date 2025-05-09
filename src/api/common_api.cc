#include "common_api.h"
#include <iostream>
std::string RandomString(std::size_t len)
{
    static constexpr char charset[] =
        "abcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist{
        0, sizeof(charset) - 2 // 最后一位为'\0'
    };

    std::string s;
    s.reserve(len);
    for (std::size_t i = 0; i < len; ++i)
    {
        s += charset[dist(rng)];
    }
    return s;
}

bool VerifyToken(const std::string &user_name,
                 const std::string &token)
{
    try
    {
        // 取出存储的 token（OptionalString 类型）
        auto &redis = get_redis();
        auto stored = redis.get(user_name);

        if (!stored)
        {
            // std::cerr << "VerifyToken: user not found\n";
            return false;
        }
        if (*stored != token)
        {
            // std::cerr << "VerifyToken: token mismatch\n";
            return false;
        }

        // 这里可以再做一次 expire
        // redis.expire(user_name, std::chrono::seconds(86400));

        return true;
    }
    catch (const sw::redis::IoError &e)
    {
        std::cerr << "Redis connection failed in VerifyToken: "
                  << e.what() << "\n";
        return false;
    }
    catch (const sw::redis::Error &e)
    {
        std::cerr << "Redis operation failed in VerifyToken: "
                  << e.what() << "\n";
        return false;
    }
    catch (...)
    {
        std::cerr << "Unknown error in VerifyToken\n";
        return false;
    }
}
