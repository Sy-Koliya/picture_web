// dbserver/dbapi/api_sharefiles_proc.h

#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include <string>

constexpr char SHARE_COUNT_KEY[] = "xxx_share_xxx_file_xxx_list_xxx_count_xxx";

using rpc::DatabaseService;
using rpc::GetShareFilesCountRequest;
using rpc::GetShareFilesCountResponse;
using rpc::GetShareFileListRequest;
using rpc::GetShareFileListResponse;
using rpc::ShareFileInfo;
using rpc::RankingFileInfo;
using rpc::GetRankingFileListRequest;
using rpc::GetRankingFileListResponse;;

// 1) GetShareFilesCount

template<>
struct ServiceMethodTraits<GetShareFilesCountRequest> {
    using ResponseType = GetShareFilesCountResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestGetShareFilesCount;
};


static int SyncGetShareFilesCount(int& total) {
    try {
        // SELECT ...
        std::string sql = std::string(
            "SELECT count FROM user_file_count WHERE user='")
            + SHARE_COUNT_KEY + "'";
        auto r = SakilaDatabase.Query(sql.c_str());
        if (!r || r->GetRowCount() == 0) {
            // 初始化
            std::string ins = std::string(
                "INSERT INTO user_file_count (user, count) VALUES('")
                + SHARE_COUNT_KEY + "', 0)";
            SakilaDatabase.Execute(ins.c_str());
            total = 0;
        } else {
            total = r->Fetch()[0].GetInt32();
        }
        return 0;
    } catch(...) {
        return 1;
    }
}

class GetShareFilesCountCall
  : public CallData<GetShareFilesCountRequest, GetShareFilesCountCall>
{
public:
    GetShareFilesCountCall(DatabaseService::AsyncService* svc,
                           grpc::ServerCompletionQueue* cq)
      : CallData<GetShareFilesCountRequest, GetShareFilesCountCall>(svc, cq) {}

    // 同步读取或初始化全局共享文件计数

    void OnRequest(const GetShareFilesCountRequest& /*req*/,
                   GetShareFilesCountResponse& resp) 
    {
        // 先排队下一个 handler

        int total = 0;
        int code  = SyncGetShareFilesCount(total);
        resp.set_code(code);
        if (code == 0) resp.set_total(total);
        rpc_finish();
    }
};

// 2) GetShareFileList

template<>
struct ServiceMethodTraits<GetShareFileListRequest> {
    using ResponseType = GetShareFileListResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestGetShareFileList;
};

class GetShareFileListCall
  : public CallData<GetShareFileListRequest, GetShareFileListCall>
{
public:
    GetShareFileListCall(DatabaseService::AsyncService* svc,
                         grpc::ServerCompletionQueue* cq)
      : CallData<GetShareFileListRequest, GetShareFileListCall>(svc, cq) {}

    void OnRequest(const GetShareFileListRequest& req,
                   GetShareFileListResponse& resp) 
    {

        // 2) 填 total
        int total = 0;
        SyncGetShareFilesCount(total);
        resp.set_total(total);

        // 3) 分页校验
        int start = req.start();
        int cnt   = req.count();
        if (start >= total) {
            resp.set_count(0);
            resp.set_code(0);
            rpc_finish();
            return;
        }
        if (start + cnt > total) cnt = total - start;


        try {
                std::string sql = std::string(R"SQL(
                SELECT 
                    u.`user`           AS user,
                    u.md5              AS md5,
                    CAST(u.create_time AS CHAR) AS create_time,
                    u.file_name        AS file_name,
                    u.shared_status    AS share_status,
                    u.pv               AS pv,
                    f.url              AS url,
                    f.size             AS size,
                    f.type             AS type
                FROM user_file_list u
                JOIN file_info f 
                ON u.md5 = f.md5
                LIMIT )SQL")
            + std::to_string(start)
            + ", "
            + std::to_string(cnt);


            auto result = SakilaDatabase.Query(sql.c_str());
            if (result) {
                uint64_t rows = result->GetRowCount();
                for (uint64_t i = 0; i < rows; ++i) {
                    auto row = result->Fetch();
                    auto* f = resp.add_files();
                    f->set_user         (row[0].GetString());
                    f->set_md5          (row[1].GetString());
                    f->set_create_time  (row[2].GetString());
                    f->set_file_name    (row[3].GetString());
                    f->set_share_status (row[4].GetInt32());
                    f->set_pv           (row[5].GetInt32());
                    f->set_url          (row[6].GetString());
                    f->set_size         (row[7].GetInt32());
                    f->set_type         (row[8].GetString());
                    result->NextRow();
                }
                resp.set_count(static_cast<int>(rows));
            } else {
                resp.set_count(0);
            }
        } catch(...) {
            resp.set_count(0);
        }

        // 5) 完成
        resp.set_code(0);
        rpc_finish();
    }
};

// 3) 
template<>
struct ServiceMethodTraits<GetRankingFileListRequest> {
    using ResponseType = GetRankingFileListResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestGetRankingFileList;
};

class GetRankingFileListCall
  : public CallData<GetRankingFileListRequest, GetRankingFileListCall>
{
public:
    GetRankingFileListCall(DatabaseService::AsyncService* svc,
                           grpc::ServerCompletionQueue* cq)
      : CallData<GetRankingFileListRequest, GetRankingFileListCall>(svc, cq) {}

    void OnRequest(const GetRankingFileListRequest& req,
                   GetRankingFileListResponse& resp) 
    {

        int total = 0;
        SyncGetShareFilesCount(total);
        resp.set_total(total);

        int start = req.start(), cnt = req.count();
        if (start + cnt > total) cnt = total - start;

        try {
            std::string sql =
                "SELECT file_name, pv "
                "FROM share_file_list "
                "ORDER BY pv DESC "
                "LIMIT " + std::to_string(start) + "," + std::to_string(cnt);
            auto r = SakilaDatabase.Query(sql.c_str());
            if (r) {
                for (int i = 0; i < r->GetRowCount(); ++i) {
                    auto row = r->Fetch();
                    auto* info = resp.add_files();
                    info->set_filename(row[0].GetString());
                    info->set_pv       (row[1].GetInt32());
                    r->NextRow();
                }
            }
        } catch(...) { /* ignore */ }

        resp.set_code(0);
        resp.set_count(resp.files_size());
        rpc_finish();
    }

};