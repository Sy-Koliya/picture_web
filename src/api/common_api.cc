#include "common_api.h"
#include "HttpConn.h"
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


/**
 * @brief  解析url query 类似 abc=123&bbb=456 字符串
 *          传入一个key,得到相应的value
 * @returns
 *          0 成功, -1 失败
 */

bool QueryParseKeyValue(const std::string &query_in,
                        const std::string &key,
                        std::string &value) {
    // 先剥离 path 部分
    auto qpos = query_in.find('?');
    const std::string query = (qpos == std::string::npos)
                              ? query_in
                              : query_in.substr(qpos + 1);

    size_t pos = 0;
    const std::string pattern = key + "=";
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        size_t len = (amp == std::string::npos ? query.size() : amp) - pos;
        if (len >= pattern.size() &&
            query.compare(pos, pattern.size(), pattern) == 0) {
            value = query.substr(pos + pattern.size(), len - pattern.size());
            return true;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return false;
}


bool SetRespToHttpConn(int fd, std::string &&  body){
   std:: string response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: " +
      std::to_string(body.size()) +
      "\r\n\r\n" + body;
  if (auto b = FindBaseSocket(fd); b)
  {
    if (auto *h = dynamic_cast<HttpConn *>(b.GetBasePtr()))
    {
      h->SetResponse(std::move(response));
      return true;
    }
  }
  return false;
}