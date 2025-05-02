#include "api_login.h"
#include "Global.h"
#include "HttpConn.h"
#include "GrpcClient.h"
#include "RedisClient.h"
#include "commom_api.h"
#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <mutex>

using nlohmann::json;
using rpc::DatabaseService;
using rpc::LoginRequest;
using rpc::LoginResponse;
using std::string;
using namespace sw::redis;

// 全局 gRPC 客户端 stub
static std::unique_ptr<
    MysqlClient<LoginRequest, LoginResponse>>
    login_client;
static std::once_flag login_init_flag;

// 线程安全初始化 stub
static void init_login_stub()
{
  auto addr = Global::Instance().get<string>("Mysql_Rpc_Server");
  auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
  login_client = std::make_unique<
      MysqlClient<LoginRequest, LoginResponse>>(channel);
}


static int setToken(const std::string& user_name, std::string& token) {
    try {
        // 1. 拿到 Redis 客户端
        auto &redis = get_redis();

        // 2. 先读一次，看 key 有没有值
        auto val = redis.get(user_name);
        if (val) {
            // key 存在，直接返回旧 token
            token = *val;
            return 0;
        }

        // 3. key 不存在，生成新的 token
        token = RandomString(32);

        // 4. 写入 Redis 并设置过期时间（86400 秒）
        redis.setex(user_name,
                    std::chrono::seconds(86400),
                    token);

        return 0;  // 写入成功
    }
    catch (const sw::redis::IoError &e) {
        std::cerr << "Redis connection failed: " << e.what() << "\n";
        return -1;
    }
    catch (const sw::redis::Error &e) {
        std::cerr << "Redis operation failed: " << e.what() << "\n";
        return -1;
    }
    catch (...) {
        std::cerr << "Unknown Redis error\n";
        return -1;
    }
}

// 协程任务：处理 /api/login
RpcTask<int> ApiUserLogin(int fd, string &post_data)
{
  if (Global::Instance().get<int>("Debug") & Debug_std)
  {
    std::cout << ", body=" << post_data << std::endl;
  }

  int code = 0;
  string user_name, pwd, token;

  // 解析 JSON
  try
  {
    auto root = json::parse(post_data);
    user_name = root.at("user").get<string>();
    pwd = root.at("pwd").get<string>();
  }
  catch (...)
  {
    code = -1; // 解析失败

    std::cout << "json parser error" << '\n';
  }

  if (code == -1)
  {
  }
  else
  {

    // rpc调用
    std::call_once(login_init_flag, init_login_stub);
    std::cout << "rpc start" << '\n';

    LoginRequest req;
    req.set_user_name(user_name);
    req.set_password(pwd);
    try
    {
      LoginResponse resp =
          co_await MysqlLoginCall(login_client.get(), std::move(req));
      code = resp.code();
    }
    catch (...)
    {
      code = -1;
    }
    std::cout << "rpc end" << '\n';
  }

  //  构造 HTTP/JSON 响应
  json out;
  if (code == 1)
  {
    if (setToken(user_name, token) != -1)
      out["token"] = token;
    std::cout << "token : " << token << '\n';
    code = 0;
    out["code"] = 0;
  }
  else
  {
    out["code"] = -1;
  }

  auto body = out.dump();

  string response =
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: " +
      std::to_string(body.size()) +
      "\r\n\r\n" + body;
  auto *conn = FindBaseSocket(fd);
  if (conn && conn->IsAlive())
  {
    if (auto *h = dynamic_cast<HttpConn *>(conn))
    {
      h->SetResponse(std::move(response));
    }
  }
  else
  {
    std::cerr << "[ApiUserLogin] invalid socket fd=" << fd << std::endl;
  }

  co_return code;
}
