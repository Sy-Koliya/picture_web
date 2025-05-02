#ifndef API_COMMOM_H
#define API_COMMOM_H
#include <string>
#include "RedisClient.h"




#define FILE_NAME_LEN (256)    //文件名字长度
#define TEMP_BUF_MAX_LEN (512) //临时缓冲区大小
#define FILE_URL_LEN (512)     //文件所存放storage的host_name长度
#define HOST_NAME_LEN (30)     //主机ip地址长度
#define USER_NAME_LEN (128)    //用户名字长度
#define TOKEN_LEN (128)        //登陆token长度
#define MD5_LEN (256)          //文件md5长度
#define PWD_LEN (256)          //密码长度
#define TIME_STRING_LEN (25)   //时间戳长度
#define SUFFIX_LEN (8)         //后缀名长度
#define PIC_NAME_LEN (10)      //图片资源名字长度
#define PIC_URL_LEN (256)      //图片资源url名字长度

#define HTTP_RESP_OK 0
#define HTTP_RESP_FAIL 1           //
#define HTTP_RESP_USER_EXIST 2     // 用户存在
#define HTTP_RESP_DEALFILE_EXIST 3 // 别人已经分享此文件
#define HTTP_RESP_TOKEN_ERR 4      //  token验证失败
#define HTTP_RESP_FILE_EXIST 5     //个人已经存储了该文件










inline std::string RandomString(std::size_t len)
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

inline bool VerifyToken(const std::string &user_name,
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

#endif
