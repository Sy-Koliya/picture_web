// api_sharefiles.cc
#include "api_sharefiles.h"
#include "mysql_rpc.grpc.pb.h"
#include "GrpcClient.h"
#include "common_api.h"
#include "RpcCoroutine.h"
#include "Global.h"
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using rpc::DatabaseService;
using rpc::GetShareFilesCountRequest;
using rpc::GetShareFilesCountResponse;
using rpc::GetShareFileListRequest;
using rpc::GetShareFileListResponse;
using rpc::GetRankingFileListRequest;
using rpc::GetRankingFileListResponse;

static std::unique_ptr<GrpcClient<DatabaseService,
    GetShareFilesCountRequest, GetShareFilesCountResponse>>
    db_count_client;
static std::unique_ptr<GrpcClient<DatabaseService,
    GetShareFileListRequest, GetShareFileListResponse>>
    db_list_client;
static std::unique_ptr<GrpcClient<DatabaseService,
    GetRankingFileListRequest, GetRankingFileListResponse>>
    db_rank_client;

static std::once_flag grpc_init_flag;
static void init_clients() {
    auto addr = Global::Instance().get<std::string>("Mysql_Rpc_Server");
    auto chan = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    db_count_client.reset(
      new GrpcClient<DatabaseService, GetShareFilesCountRequest, GetShareFilesCountResponse>(chan));
    db_list_client.reset(
      new GrpcClient<DatabaseService, GetShareFileListRequest, GetShareFileListResponse>(chan));
    db_rank_client.reset(
      new GrpcClient<DatabaseService, GetRankingFileListRequest, GetRankingFileListResponse>(chan));
}

static bool parse_list_args(const json &in, int &start, int &count) {
    if (!in.contains("start") || !in.contains("count")) return false;
    start = in["start"].get<int>();
    count = in["count"].get<int>();
    return true;
}

RpcTask<int> ApiSharefiles(int fd,
                           const std::string &post_data,
                           const std::string &url) 
{
    std::call_once(grpc_init_flag, init_clients);
    std::string cmd;
    QueryParseKeyValue(url.c_str(), "cmd", cmd);

    json in;
    try { in = json::parse(post_data); }
    catch(...) { in = json::object(); }

    json out;
    int code = 1;

    if (cmd == "count") {
        // 获取共享文件总数
        GetShareFilesCountRequest req;
        auto resp = co_await MysqlGetShareFilesCountCall(db_count_client.get(), req);
        code = resp.code();
        out["code"]  = code;
        out["total"] = resp.total();
    }
    else {
        int start=0, cnt=0;
        if (!parse_list_args(in, start, cnt)) {
            out["code"] = 1;
        } 
        else if (cmd == "normal") {
            GetShareFileListRequest req;
            req.set_start(start);
            req.set_count(cnt);
            auto resp = co_await MysqlGetShareFileListCall(db_list_client.get(), req);
            code = resp.code();
            out["code"]  = code;
            out["total"] = resp.total();
            out["count"] = resp.count();
            if (code == 0) {
                json arr = json::array();
                for (auto &f : resp.files()) {
                    json v;
                    v["user"]         = f.user();
                    v["md5"]          = f.md5();
                    v["file_name"]    = f.file_name();
                    v["share_status"] = f.share_status();
                    v["pv"]           = f.pv();
                    v["create_time"]  = f.create_time();
                    v["url"]          = f.url();
                    v["size"]         = f.size();
                    v["type"]         = f.type();
                    arr.push_back(v);
                }
                out["files"] = std::move(arr);
            }
        }
        else if (cmd == "pvdesc") {
            GetRankingFileListRequest req;
            req.set_start(start);
            req.set_count(cnt);
            auto resp = co_await MysqlGetRankingFileListCall(db_rank_client.get(), req);
            code = resp.code();
            out["code"]  = code;
            out["total"] = resp.total();
            out["count"] = resp.count();
            if (code == 0) {
                json arr = json::array();
                for (auto &f : resp.files()) {
                    json v;
                    v["filename"] = f.filename();
                    v["pv"]       = f.pv();
                    arr.push_back(v);
                }
                out["files"] = std::move(arr);
            }
        }else {
            out["code"] = 1;
        }
    }
    // 输出 HTTP 响应
    SetRespToHttpConn(fd, out.dump());
    co_return 1;
}
