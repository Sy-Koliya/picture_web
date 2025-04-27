

#include "api_register.h"
#include "Global.h"
#include "HttpConn.h"
#include "nlohmann/json.hpp"
#include <grpcpp/grpcpp.h>
#include <string>
#include <mutex>
#include <thread>

using json = nlohmann::json;
using std::string;
using rpc::RegisterRequest;
using rpc::RegisterResponse;
using rpc::DatabaseService;



//初始化一个全局Client
static std::unique_ptr<MysqlClient<RegisterRequest,RegisterResponse>> mysql_client;
static std::once_flag init_flag;

void init_gstub() {
    auto channel = grpc::CreateChannel(
        Global::Instance().get<std::string>("Mysql_Rpc_Server"),
            grpc::InsecureChannelCredentials());
    mysql_client= std::make_unique< MysqlClient<RegisterRequest,RegisterResponse>>(channel);        
}

// 解析注册 JSON
int decodeRegisterJson(const string &str_json,
                       string &user_name,
                       string &nick_name,
                       string &password,
                       string &phone,
                       string &email)
{
    json root;
    try {
        root = json::parse(str_json);
    } catch (const json::parse_error &e) {
        return -1;
    }
    if (!root.contains("user_name") || !root.contains("nick_name") || !root.contains("password")) {
        return -2;
    }
    user_name = root["user_name"].get<string>();
    nick_name = root["nick_name"].get<string>();
    password       = root["password"].get<string>();
    if (root.contains("phone")) phone = root["phone"].get<string>();
    if (root.contains("email")) email = root["email"].get<string>();
    return 0;
}

// 构造响应 JSON
int encodeRegisterJson(int code, string &out_json)
{
    json root;
    root["code"] = code;
    out_json = root.dump();
    return 0;
}



// 协程任务：处理注册 API，返回业务 code
RpcTask<int> ApiRegisterUser(int fd,uint32_t conn_uui, string &post_data)
{
    // 1. 日志输出
    if (Global::Instance().get<int>("Debug") & Debug_std) {
        std::cout << "[ApiRegisterUser] uuid=" << conn_uui
                  << ", data=" << post_data << std::endl;
    }

    int code = 0;
    string user_name, nick_name, password, phone, email;
    
    // 2. 解析参数
    if (post_data.empty()) {
        code = -1; // 请求体为空
    } else {
        int ret = decodeRegisterJson(post_data, user_name, nick_name, password, phone, email);
        if (ret < 0) {
            code = ret;  // -1/ -2
        } else {
            // 3. 初始化 gRPC Stub
            std::cout<<"rpc start"<<'\n';
            std::call_once(init_flag, init_gstub);

            // 4. 发起异步 RPC
            RegisterRequest rpc_req;
            rpc_req.set_nick_name(nick_name);
            rpc_req.set_user_name(user_name);
            rpc_req.set_password(password);
            rpc_req.set_phone(phone);
            rpc_req.set_email(email);
            try{
            RegisterResponse rpc_resp = co_await MysqlRegisterCall(
               mysql_client.get(), std::move(rpc_req));
                code = rpc_resp.code();
            }catch(...){
                //close();
            }
            std::cout<<"rpc end"<<'\n';

        }
    }
    std::cout<<"code  "<< code<<'\n';
    
    // 6. 构造 HTTP 响应
    // string body_json;
    // encodeRegisterJson(code, body_json);
    // string response =
    //     "HTTP/1.1 200 OK\r\n"
    //     "Connection: close\r\n"
    //     "Content-Type: application/json; charset=utf-8\r\n"
    //     "Content-Length: " + std::to_string(body_json.size()) +
    //     "\r\n\r\n" + body_json;

    // // 7. 发送 HTTP 响应
    // BaseSocket* b = FindBaseSocket(socket_fd);
    // if (b && b->IsAlive()) {
    //     if (auto* h = dynamic_cast<HttpConn*>(b)) {
    //         h->SetResponse(std::move(response));
    //     }
    // } else {
    //     std::cerr << "[ApiRegisterUser] Socket 无效 fd=" << socket_fd << std::endl;
    // }

    // 8. 返回 code
    co_return code;
}
