#include "api_myfiles.h"
#include "GrpcClient.h"
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "common_api.h"
#include "HttpConn.h"
#include <nlohmann/json.hpp>
#include <mutex>

using nlohmann::json;
using rpc::DatabaseService;
using rpc::CountRequest;
using rpc::CountResponse;
using rpc::FilesListRequest;
using rpc::FilesListResponse;
using rpc::OrderBy;
using std::string;
using namespace sw::redis;

// ---- 全局 gRPC 客户端 & 初始化 ----
static std::unique_ptr<
    GrpcClient<DatabaseService, CountRequest, CountResponse>>
    count_client;
static std::unique_ptr<
    GrpcClient<DatabaseService, FilesListRequest, FilesListResponse>>
    list_client;
static std::once_flag grpc_init_flag;

static void init_grpc_clients() {
    auto addr = Global::Instance().get<string>("Mysql_Rpc_Server");
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    count_client = std::make_unique<
        GrpcClient<DatabaseService, CountRequest, CountResponse>>(channel);
    list_client  = std::make_unique<
        GrpcClient<DatabaseService, FilesListRequest, FilesListResponse>>(channel);
}
RpcTask<int> ApiMyfiles(int fd,
                        const std::string &post_data,
                        const std::string &url) {
    // 1) 确保 gRPC 客户端只初始化一次
    std::call_once(grpc_init_flag, init_grpc_clients);

    // 2) 从 URL 里取 cmd
    std::string cmd;
    QueryParseKeyValue(url, "cmd", cmd);

    // 3) 准备 JSON 容器
    nlohmann::json in, out;
    int    code   = 0;
    int    start  = 0;
    int    limit  = 0;      // 对应前端的 "count"
    std::string user, token;

    // —— JSON 解析 ——  
    try {
        in     = nlohmann::json::parse(post_data);
        user   = in.value("user", "");
        token  = in.value("token", "");
        start  = in.value("start", 0);
        limit  = in.value("count", 0);  // 
    } catch (...) {
        code        = -1;
        out["code"] = code;
        goto RESPOND;
    }

    // —— Token 校验 ——  
    if (!VerifyToken(user, token)) {
        code        = 1;
        out["code"] = code;
        goto RESPOND;
    }
    std::cout<<cmd<<'\n';
    // —— count 分支 ——  
    if (cmd == "count") {
        auto &redis = get_redis();
        std::string key = user + ":file_count";

        if (auto v = redis.get(key)) {
            code         = 0;
            out["code"]  = code;
            out["total"] = std::stoi(*v);
            goto RESPOND;
        }

        // 构造并发起 RPC
        rpc::CountRequest  creq;
        creq.set_user(user);
        creq.set_token(token);
        rpc::CountResponse cresp;
        try {
            cresp = co_await MysqlGetUserFilesCountCall(
                        count_client.get(), std::move(creq));
        } catch (...) {
            code        = -1;
            out["code"] = code;
            goto RESPOND;
        }

        code        = cresp.code();
        out["code"] = code;
        if (code == 0) {
            out["total"] = cresp.count();
            std::cout<<cresp.count()<<'\n';
            // 缓存一小时
            redis.setex(key,
                        std::chrono::seconds(3600),
                        std::to_string(cresp.count()));
        }
        goto RESPOND;
    }
    else{
        rpc::FilesListRequest lreq;
        lreq.set_user_id(user);
        lreq.set_token(token);
        lreq.set_start(start);
        lreq.set_limit(limit);

        // URL cmd -> OrderBy 枚举
        rpc::OrderBy ob = rpc::OrderBy::NORMAL;
        if (cmd == "pvasc")      ob = rpc::OrderBy::PV_ASC;
        else if (cmd == "pvdesc") ob = rpc::OrderBy::PV_DESC;
        lreq.set_order_by(ob);

        rpc::FilesListResponse lresp;
        try {
            lresp = co_await MysqlGetUserFileListCall(
                        list_client.get(), std::move(lreq));
        } catch (...) {
            code        = -1;
            out["code"] = code;
            goto RESPOND;
        }

        // 填充返回 JSON
        code          = lresp.code();
        out["code"]   = code;
        out["count"]  = lresp.count();
        out["total"]  = lresp.total();
        out["files"]  = nlohmann::json::array();
        for (auto &f : lresp.files()) {
            out["files"].push_back({
                {"user",        f.user_id()},
                {"md5",         f.file_md5()},
                {"create_time", f.created_at()},
                {"file_name",   f.filename()},
                {"share_status",f.is_shared()},
                {"pv",          f.view_count()},
                {"url",         f.file_url()},
                {"size",        f.file_size()},
                {"type",        f.file_type()}
            });
                //     std::cout
                // << "user_id="    << f.user_id()
                // << " md5="       << f.file_md5()
                // << " created_at="<< f.created_at()
                // << " filename="  << f.filename()
                // << " is_shared=" << (f.is_shared() ? "true" : "false")
                // << " view_count="<< f.view_count()
                // << " file_url="  << f.file_url()
                // << " file_size=" << f.file_size()
                // << " file_type=" << f.file_type()
                // << std::endl;
        }
    }

RESPOND:
    {
        // 统一构造 HTTP/JSON 响应
        std::string body = out.dump();
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Connection: close\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
            body;
        std::cout<<response<<'\n';
        if (auto sock = FindBaseSocket(fd); sock) {
            if (auto *h = dynamic_cast<HttpConn*>(sock.GetBasePtr())) {
                h->SetResponse(std::move(response));
            }
        }
    }

    co_return code;
}
