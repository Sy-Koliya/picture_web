#include "api_sharepicture.h"
#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "GrpcClient.h"
#include "common_api.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using rpc::DatabaseService;

// —— gRPC 客户端单例 —— 
static std::unique_ptr<GrpcClient<DatabaseService, rpc::SharePictureRequest,      rpc::SharePictureResponse>>      db_share_pic_client;
static std::unique_ptr<GrpcClient<DatabaseService, rpc::GetSharePicturesCountRequest,  rpc::GetSharePicturesCountResponse>> db_sp_count_client;
static std::unique_ptr<GrpcClient<DatabaseService, rpc::GetSharePicturesListRequest,   rpc::GetSharePicturesListResponse>>  db_sp_list_client;
static std::unique_ptr<GrpcClient<DatabaseService, rpc::CancelSharePictureRequest,   rpc::CancelSharePictureResponse>>   db_sp_cancel_client;
static std::unique_ptr<GrpcClient<DatabaseService, rpc::BrowsePictureRequest,      rpc::BrowsePictureResponse>>      db_sp_browse_client;

static std::once_flag grpc_init_flag;
static void init_share_picture_clients() {
    auto addr    = Global::Instance().get<std::string>("Mysql_Rpc_Server");
    auto channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    db_share_pic_client.reset(new GrpcClient<DatabaseService, rpc::SharePictureRequest,      rpc::SharePictureResponse>(channel));
    db_sp_count_client.reset(new GrpcClient<DatabaseService, rpc::GetSharePicturesCountRequest,  rpc::GetSharePicturesCountResponse>(channel));
    db_sp_list_client.reset(new GrpcClient<DatabaseService, rpc::GetSharePicturesListRequest,   rpc::GetSharePicturesListResponse>(channel));
    db_sp_cancel_client.reset(new GrpcClient<DatabaseService, rpc::CancelSharePictureRequest,   rpc::CancelSharePictureResponse>(channel));
    db_sp_browse_client.reset(new GrpcClient<DatabaseService, rpc::BrowsePictureRequest,      rpc::BrowsePictureResponse>(channel));
}


static constexpr int debug = 1;


RpcTask<int> ApiSharepicture(int fd,
                             const std::string &post_data,
                             const std::string &url) 
{
    std::call_once(grpc_init_flag, init_share_picture_clients);

    // 1) 解析 cmd
    std::string cmd;
    if (! QueryParseKeyValue(url.c_str(), "cmd", cmd)) {
        json e; e["code"] = 1;
        SetRespToHttpConn(fd, e.dump());
        co_return 1;
    }
    if(debug==1){
        std::cout<<"[Debug]  api_sharepicture:"<<cmd<<std::endl;
    }
    // 2) 解析 JSON
    json in;
    try {
        in = json::parse(post_data);
    } catch (...) {
        json e; e["code"] = 1;
        SetRespToHttpConn(fd, e.dump());
        co_return 1;
    }

    json out;
    if (cmd == "share") {
        if(debug==1){
            std::cout<<"[Debug]  api_sharepicture: share"<<std::endl;
        }
        // — 分享图片 —
        if (!in.contains("user") || !in.contains("token") || 
            !in.contains("md5")  || !in.contains("filename")) 
        {
            out["code"] = 1;
        } 
        else 
        {
            if(debug == 1) {
                std::cout << "[Debug]  api_sharepicture: rpc start" << std::endl;
            }
            rpc::SharePictureRequest req;
            req.set_user(    in["user"].get<std::string>());
            req.set_md5(     in["md5"].get<std::string>());
            req.set_filename(in["filename"].get<std::string>());

            auto resp = co_await MysqlSharePicture(db_share_pic_client.get(), std::move(req));
            out["code"]   = resp.code();
            if (resp.code() == 0) out["urlmd5"] = resp.urlmd5();
            if(debug == 1) {
                std::cout << "[Debug]  api_sharepicture: rpc end" << std::endl;
            }
        }
    }
    else if (cmd == "normal") {
        // — 获取列表 —
        if (!in.contains("user") || !in.contains("token") ||
            !in.contains("start")|| !in.contains("count"))
        {
            out["code"] = 1;
        }
        else
        {
            std::string user = in["user"];
            int start = in["start"];
            int cnt   = in["count"];

            // a) 先拿总数
            if(debug==1){
                std::cout<<"[Debug]  api_sharepicture: rpc start"<<std::endl;
            }
            rpc::GetSharePicturesCountRequest  r0; r0.set_user(user);
            auto c0 = co_await MysqlGetSharePicturesCount(db_sp_count_client.get(), std::move(r0));
            if (c0.code() != 0) {
                out["code"] = 1;
            } else {
                int total = c0.total();
                // b) 再拿列表
                rpc::GetSharePicturesListRequest r1;
                r1.set_user(user); r1.set_start(start); r1.set_count(cnt);
                auto c1 = co_await MysqlGetSharePicturesList(db_sp_list_client.get(), std::move(r1));

                if (c1.code() != 0) {
                    out["code"] = 1;
                } else {
                    out["code"]  = 0;
                    out["total"] = total;
                    out["count"] = c1.files_size();
                    out["files"] = json::array();
                    for (auto &f : c1.files()) {
                        json fi;
                        fi["user"]        = f.user();
                        fi["filemd5"]     = f.filemd5();
                        fi["file_name"]   = f.file_name();
                        fi["urlmd5"]      = f.urlmd5();
                        fi["pv"]          = f.pv();
                        fi["create_time"] = f.create_time();
                        fi["size"]        = f.size();
                        out["files"].push_back(std::move(fi));
                    }
                }
            }
            if(debug==1){
                std::cout<<"[Debug]  api_sharepicture: rpc end"<<std::endl;
            }
        }
    }
    else if (cmd == "cancel") {
        // — 取消分享 —
        if(debug==1){
            std::cout<<"[Debug]  api_sharepicture: cancel"<<std::endl;
        }
        if (!in.contains("user") || !in.contains("urlmd5")) {
            out["code"] = 1;
        } else {
            rpc::CancelSharePictureRequest req;
            req.set_user(   in["user"].get<std::string>());
            req.set_urlmd5( in["urlmd5"].get<std::string>());
            auto resp = co_await MysqlCancelSharePicture(db_sp_cancel_client.get(), std::move(req));
            out["code"] = resp.code();
        }
        if(debug==1){
            std::cout<<"[Debug]  api_sharepicture: cancel end"<<std::endl;
        }
    }
    else if (cmd == "browse") {
        if(debug==1){
            std::cout<<"[Debug]  api_sharepicture: browse"<<std::endl;
        }
        // — 浏览并自增 pv —
        if (!in.contains("urlmd5")) {
            out["code"] = 1;
        } else {
            rpc::BrowsePictureRequest req;
            req.set_urlmd5(in["urlmd5"].get<std::string>());
            auto resp = co_await MysqlBrowsePicture(db_sp_browse_client.get(), std::move(req));
            out["code"] = resp.code();
            if (resp.code() == 0) {
                out["pv"]   = resp.pv();
                out["url"]  = resp.url();
                out["user"] = resp.user();
                out["time"] = resp.time();
            }
        }
        if(debug==1){
            std::cout<<"[Debug]  api_sharepicture: browse end"<<std::endl;
        }
    }
    else {
        out["code"] = 1;
    }

    // 3) 下发 HTTP 响应
    SetRespToHttpConn(fd, out.dump());
    co_return 1;
}
