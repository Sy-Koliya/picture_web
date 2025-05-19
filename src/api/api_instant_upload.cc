// api_md5.cc

#include "api_instant_upload.h"
#include "Global.h"
#include "HttpConn.h"
#include "nlohmann/json.hpp"
#include "common_api.h"
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <thread>

using json   = nlohmann::json;
using std::string;
using rpc::Md5Request;
using rpc::Md5Response;
using rpc::DatabaseService;

// —— 全局 gRPC 客户端 —— 
static std::unique_ptr<
    GrpcClient<rpc::DatabaseService,
               rpc::Md5Request,
               rpc::Md5Response>
> md5_client;
static std::once_flag md5_init_flag;

// 真正的初始化函数，只执行一次
static void init_md5_client() {
    std::string endpoint = Global::Instance().get<std::string>("Mysql_Rpc_Server");
    if (Global::Instance().get<int>("Debug") & Debug_std) {
        std::cout << "[DEBUG] MD5 gRPC target = '" << endpoint << "'\n";
    }
    auto channel = grpc::CreateChannel(endpoint,
                                       grpc::InsecureChannelCredentials());
    md5_client = std::make_unique<
        GrpcClient<rpc::DatabaseService,
                   rpc::Md5Request,
                   rpc::Md5Response>
    >(channel);
}
// —— JSON 解析 / 序列化 —— 
static int decodeMd5Json(const string &str_json,
                         string &user,
                         string &token,
                         string &md5,
                         string &filename)
{
    json j;
    try {
        j = json::parse(str_json);
    } catch (const json::parse_error &) {
        return -1;
    }

    if (!j.contains("user") || !j.contains("token")
     || !j.contains("md5")  || !j.contains("filename"))
    {
        return -2;
    }

    user     = j["user"].get<string>();
    token    = j["token"].get<string>();
    md5      = j["md5"].get<string>();
    filename = j["filename"].get<string>();
    return 0;
}

static int encodeMd5Json(int code, string &out_json)
{
    json j;
    j["code"] = code;
    out_json  = j.dump();
    return 0;
}

// —— HTTP 协程任务 —— 
RpcTask<int> ApiInstantUpload(int fd, const string &post_data,const string& /*uri*/)
{
    if (Global::Instance().get<int>("Debug") & Debug_std) {
        std::cout << "[ApiInstantUpload] post_data=" << post_data << "\n";
    }

    int code = 0;
    string user, token, md5, filename;

    if (post_data.empty()) {
        code = 1;  // 空请求
    } else {
        int ret = decodeMd5Json(post_data, user, token, md5, filename);
        if (ret < 0) {
            code = 1;  // 解析失败
        } else {
            if (!VerifyToken(user,token)) {
                code = 4; // Token 错误
            } else {
                // 初始化 gRPC 客户端
                std::call_once(md5_init_flag, init_md5_client);

                // 构造 RPC 请求
                Md5Request rpc_req;
                rpc_req.set_user(user);
                rpc_req.set_md5(md5);
                rpc_req.set_filename(filename);

                try {
                    // 发起异步调用，挂起协程
                    Md5Response rpc_resp =
                        co_await MysqlInstantUploadCall(md5_client.get(),
                                                        std::move(rpc_req));
                    code = rpc_resp.code();
                } catch (const std::exception &e) {
                    std::cerr << "[ApiInstantUpload] RPC exception: " << e.what() << "\n";
                    code = 1;  // 调用失败
                }
            }
        }
    }

    // 构造并发送 HTTP 响应
    string body;
    encodeMd5Json(code, body);

    string response =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(body.size()) +
        "\r\n\r\n" +
        body;

    if (auto b = FindBaseSocket(fd); b) {
        
        if (auto *h = dynamic_cast<HttpConn*>(b.GetBasePtr())) {
            h->SetResponse(std::move(response));
        }
    } else {
        std::cerr << "[ApiInstantUpload] Socket 无效, fd=" << fd << "\n";
    }

    co_return code;
}
