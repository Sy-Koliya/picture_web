// dbapi/api_sharefiles_db.h

#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"

constexpr char SHARE_COUNT_KEY[] = "xxx_share_xxx_file_xxx_list_xxx_count_xxx";

using rpc::DatabaseService;
using rpc::GetShareFilesCountRequest;
using rpc::GetShareFilesCountResponse;
using rpc::GetShareFileListRequest;
using rpc::GetShareFileListResponse;
using rpc::GetRankingFileListRequest;
using rpc::GetRankingFileListResponse;
using rpc::ShareFileInfo;
using rpc::RankingFileInfo;

// 1) GetShareFilesCount
template<>
struct ServiceMethodTraits<GetShareFilesCountRequest> {
    using ResponseType = GetShareFilesCountResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestGetShareFilesCount;
};

class GetShareFilesCountCall
  : public CallData<GetShareFilesCountRequest, GetShareFilesCountCall>
{
public:
    GetShareFilesCountCall(rpc::DatabaseService::AsyncService* svc,
                           grpc::ServerCompletionQueue* cq)
      : CallData(svc, cq) {}

    // 同步执行：读取或初始化全局共享文件计数
    int SyncGetShareFilesCount(int& total) {
        try {
            std::string sql =
              "SELECT count FROM user_file_count WHERE user='" +
              std::string(SHARE_COUNT_KEY) + "'";
            auto r = SakilaDatabase.Query(sql.c_str());
            if (!r || r->GetRowCount() == 0) {
                // 第一次请求时，初始化
                std::string ins =
                  "INSERT INTO user_file_count (user, count) VALUES('" +
                  std::string(SHARE_COUNT_KEY) + "', 0)";
                SakilaDatabase.Execute(ins.c_str());
                total = 0;
            } else {
                total = r->Fetch()[0].GetInt32();
            }
            return 0;
        } catch (...) {
            return 1;
        }
    }

    void OnRequest(const GetShareFilesCountRequest& /*req*/,
                   GetShareFilesCountResponse& resp)
    {
        int total = 0;
        int code = SyncGetShareFilesCount(total);
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
    GetShareFileListCall(rpc::DatabaseService::AsyncService* svc,
                         grpc::ServerCompletionQueue* cq)
      : CallData(svc, cq) {}

    void OnRequest(const GetShareFileListRequest& req,
                   GetShareFileListResponse& resp)
    {
        // 总数先填充
        int total = 0;
        SyncGetShareFilesCount(total);
        resp.set_total(total);

        // 分页查询
        int start = req.start(), cnt = req.count();
        try {
            std::string sql =
              "SELECT s.user, s.md5, s.file_name, s.share_status, s.pv, s.create_time, "
              "f.url, f.size, f.type "
              "FROM share_file_list s "
              " JOIN file_info f ON f.md5 = s.md5 "
              "LIMIT " + std::to_string(start) + "," + std::to_string(cnt);
            auto r = SakilaDatabase.Query(sql.c_str());
            if (r) {
                for (int i = 0; i < r->GetRowCount(); ++i) {
                    auto row = r->Fetch();
                    auto* info = resp.add_files();
                    info->set_user        (row[0].GetString());
                    info->set_md5         (row[1].GetString());
                    info->set_file_name   (row[2].GetString());
                    info->set_share_status(row[3].GetInt32());
                    info->set_pv          (row[4].GetInt32());
                    info->set_create_time (row[5].GetString());
                    info->set_url         (row[6].GetString());
                    info->set_size        (row[7].GetInt32());
                    info->set_type        (row[8].GetString());
                }
            }
        } catch (...) { /* 忽略 SQL 错误 */ }

        resp.set_code (0);
        resp.set_count(resp.files_size());
        rpc_finish();
    }

private:
    // 复用前面的计数方法
    int SyncGetShareFilesCount(int& total) {
        // 直接调用上一类里的逻辑
        return GetShareFilesCountCall(nullptr, nullptr)
                 .SyncGetShareFilesCount(total);
    }
};

// 3) GetRankingFileList
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
    GetRankingFileListCall(rpc::DatabaseService::AsyncService* svc,
                           grpc::ServerCompletionQueue* cq)
      : CallData(svc, cq) {}

    void OnRequest(const GetRankingFileListRequest& req,
                   GetRankingFileListResponse& resp)
    {
        // 填充总数
        int total = 0;
        SyncGetShareFilesCount(total);
        resp.set_total(total);

        // 排行查询
        int start = req.start(), cnt = req.count();
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
                    info->set_pv      (row[1].GetInt32());
                }
            }
        } catch (...) { /* 忽略 */ }

        resp.set_code (0);
        resp.set_count(resp.files_size());
        rpc_finish();
    }

private:
    int SyncGetShareFilesCount(int& total) {
        return GetShareFilesCountCall(nullptr, nullptr)
                 .SyncGetShareFilesCount(total);
    }
};
