#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include <string>

using rpc::DatabaseService;
using rpc::PvFileRequest;
using rpc::PvFileResponse;


#define SQL_MAX_LEN 1024;

template <>
struct ServiceMethodTraits<PvFileRequest>
{
    using ResponseType = PvFileResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestPvFile;
};
class PvFileCall
    : public CallData<PvFileRequest, PvFileCall>
{
public:
    PvFileCall(rpc::DatabaseService::AsyncService *svc,
               grpc::ServerCompletionQueue *cq)
        : CallData<PvFileRequest, PvFileCall>(svc, cq) {}

     int SyncPvFile(const std::string& user,
                   const std::string& md5,
                   const std::string& filename)
    {
        try {
            // 1) 查询当前 pv
            std::string sql0 =
                "SELECT pv FROM user_file_list "
                "WHERE user='"     + user +
                "' AND md5='"      + md5  +
                "' AND file_name='" + filename + "'";
            auto r0 = SakilaDatabase.Query(sql0.c_str());
            if (!r0 || r0->GetRowCount() == 0) {
                return 1;
            }
            int pv = r0->Fetch()[0].GetInt32();

            // 2) 更新 pv+1
            std::string sql1 =
                "UPDATE user_file_list SET pv=" + std::to_string(pv + 1) +
                " WHERE user='"     + user +
                "' AND md5='"      + md5  +
                "' AND file_name='" + filename + "'";
            SakilaDatabase.Execute(sql1.c_str());

            return 0;
        }
        catch (...) {
            return 1;
        }
    }

    void OnRequest(const PvFileRequest &req,
                   PvFileResponse &reply)
    {
        int code = SyncPvFile(
            req.user(),
            req.md5(),
            req.filename());
        reply.set_code(code);
        rpc_finish();
    }
};