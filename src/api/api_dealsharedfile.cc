#include "api_dealsharedfile.h"
#include "common_api.h"
#include "RedisClient.h"
#include <nlohmann/json.hpp>
#include "grpcpp/grpcpp.h"
#include "mysql_rpc.grpc.pb.h"
#include "RpcCoroutine.h"
#include <mutex>

using rpc::DatabaseService;
using rpc::CancelShareFileRequest;
using rpc::CancelShareFileResponse;
using rpc::SaveFileRequest;
using rpc::SaveFileResponse;
using rpc::PvShareFileRequest;
using rpc::PvShareFileResponse;
using nlohmann::json;

// gRPC 客户端
static std::unique_ptr<GrpcClient<DatabaseService,
                                  CancelShareFileRequest,
                                  CancelShareFileResponse>> db_cancel_client;
static std::unique_ptr<GrpcClient<DatabaseService,
                                  SaveFileRequest,
                                  SaveFileResponse>>        db_save_client;
static std::unique_ptr<GrpcClient<DatabaseService,
                                  PvShareFileRequest,
                                  PvShareFileResponse>>     db_pv_client;
static std::once_flag grpc_init_flag;

static void init_sharefile_clients() {
    auto addr = Global::Instance().get<std::string>("Rpc_Mysql_Server");
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    db_cancel_client.reset(new GrpcClient<DatabaseService,
                                          CancelShareFileRequest,
                                          CancelShareFileResponse>(chan));
    db_save_client.reset(  new GrpcClient<DatabaseService,
                                          SaveFileRequest,
                                          SaveFileResponse>(chan));
    db_pv_client.reset(    new GrpcClient<DatabaseService,
                                          PvShareFileRequest,
                                          PvShareFileResponse>(chan));
}
int decodeDealsharefileJson(const std::string &str_json,
                            std::string &user_name,
                            std::string &md5,
                            std::string &filename)
{
    json root;
    try {
        root = json::parse(str_json);
    }
    catch (const json::parse_error &e) {
        return -1;
    }
    // user
    if (!root.contains("user")) {
        return -1;
    }
    user_name = root["user"].get<std::string>();

    // md5
    if (!root.contains("md5")) {
        return -1;
    }
    md5 = root["md5"].get<std::string>();

    // filename
    if (!root.contains("filename") ) {
        return -1;
    }
    filename = root["filename"].get<std::string>();
    return 0;
}


RpcTask<int> ApiDealsharefile(int fd,
                              const std::string &post_data,
                              const std::string &url) {
    std::string cmd, user, md5, filename;
    int code = 1;

    if(decodeDealsharefileJson(post_data, user, md5, filename)==-1){
        std::cerr << "json parser error" << '\n';
        code = 1;
        goto RESP;
    }
    QueryParseKeyValue(url, "cmd", cmd);

    std::call_once(grpc_init_flag, init_sharefile_clients);

    try{
    if (cmd == "cancel") {
        CancelShareFileRequest req;
        req.set_user(user);
        req.set_md5(md5);
        req.set_filename(filename);
        auto resp = co_await MysqlCancelShareFile(db_cancel_client.get(), std::move(req));
        code = resp.code();
    }
    else if (cmd == "save") {
        SaveFileRequest req;
        req.set_user(user);
        req.set_md5(md5);
        req.set_filename(filename);
        auto resp = co_await MysqlSaveFile(db_save_client.get(), std::move(req));
        code = resp.code();
    }
    else if (cmd == "pv") {
        PvShareFileRequest req;
        req.set_user(user);
        req.set_md5(md5);
        req.set_filename(filename);
        auto resp = co_await MysqlPvShareFile(db_pv_client.get(), std::move(req));
        code = resp.code();
    }
    }
    catch (const std::exception &e) {
        std::cerr << "gRPC error: " << e.what() << std::endl;
        code = 1; // gRPC 错误
    }
RESP:

    json ht_resp;
    ht_resp["code"] = code;
    SetRespToHttpConn(fd, ht_resp.dump());
    co_return 1;
}
