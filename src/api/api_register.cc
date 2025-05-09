

#include "api_register.h"
#include "Global.h"
#include "HttpConn.h"
#include "nlohmann/json.hpp"
#include <grpcpp/grpcpp.h>
#include <string>
#include <mutex>
#include <thread>

using json = nlohmann::json;
using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;
using std::string;

// 初始化一个全局Client
static std::unique_ptr<
    GrpcClient<rpc::DatabaseService,
               rpc::RegisterRequest,
               rpc::RegisterResponse>>
    register_client;
static std::once_flag register_init_flag;

// 真正的初始化函数，只执行一次
static void init_register_client()
{
    // 从配置读取地址
    std::string addr = Global::Instance().get<std::string>("Mysql_Rpc_Server");
    if (Global::Instance().get<int>("Debug") & Debug_std)
    {
        std::cout << "[DEBUG] gRPC target = '" << addr << "'\n";
    }
    // 创建 Channel
    auto channel = grpc::CreateChannel(addr,
                                       grpc::InsecureChannelCredentials());
    // 用通用 GrpcClient 实例化
    register_client = std::make_unique<
        GrpcClient<rpc::DatabaseService,
                   rpc::RegisterRequest,
                   rpc::RegisterResponse>>(channel);
}

// 解析注册 JSON
static int decodeRegisterJson(const string &str_json,
                              string &user_name,
                              string &nick_name,
                              string &password,
                              string &phone,
                              string &email)
{
    json root;
    try
    {
        root = json::parse(str_json);
    }
    catch (const json::parse_error &e)
    {
        return -1;
    }
    if (!root.contains("userName") || !root.contains("nickName") || !root.contains("firstPwd"))
    {
        return -2;
    }
    user_name = root["userName"].get<string>();
    nick_name = root["nickName"].get<string>();
    password = root["firstPwd"].get<string>();
    if (root.contains("phone"))
        phone = root["phone"].get<string>();
    if (root.contains("email"))
        email = root["email"].get<string>();
    return 0;
}

// 构造响应 JSON
static int encodeRegisterJson(int code, string &out_json)
{
    json root;
    root["code"] = code;
    out_json = root.dump();
    return 0;
}

// 协程任务：处理注册 API，返回业务 code
RpcTask<int> ApiRegisterUser(int fd, const string &post_data, const string & /*uri*/)
{
    if (Global::Instance().get<int>("Debug") & Debug_std)
    {
        std::cout << ", data=" << post_data << std::endl;
    }
    std::call_once(register_init_flag, init_register_client);

    int code = 0;
    string user_name, nick_name, password, phone, email;

    if (post_data.empty())
    {
        code = -1; // 请求体为空
    }
    else
    {
        int ret = decodeRegisterJson(post_data, user_name, nick_name, password, phone, email);
        if (ret < 0)
        {
            std::cout << "json parser faild" << '\n';
            code = ret; // -1/ -2
        }
        else
        {
            std::cout << "rpc start" << '\n';
            RegisterRequest rpc_req;
            rpc_req.set_nick_name(nick_name);
            rpc_req.set_user_name(user_name);
            rpc_req.set_password(password);
            rpc_req.set_phone(phone);
            rpc_req.set_email(email);
            try
            {
                RegisterResponse rpc_resp = co_await MysqlRegisterCall(
                    register_client.get(), std::move(rpc_req));
                code = rpc_resp.code();
            }
            catch (...)
            {
                // close();
            }
            std::cout << "rpc end" << '\n';
        }
    }
    if (code < 0)
        code = 1;
    // 构造 HTTP 响应
    string body_json;
    encodeRegisterJson(code, body_json);
    string response =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: " +
        std::to_string(body_json.size()) +
        "\r\n\r\n" + body_json;

    if (auto b = FindBaseSocket(fd); b)
    {
        if (auto *h = dynamic_cast<HttpConn *>(b.GetBasePtr()))
        {
            h->SetResponse(std::move(response));
        }
    }
    else
    {
        std::cerr << "[ApiRegisterUser] Socket 无效 fd=" << fd << std::endl;
    }

    co_return code;
}
