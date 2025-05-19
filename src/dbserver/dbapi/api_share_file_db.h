#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include <string>
#include "tools.h"


using rpc::DatabaseService;
using rpc::ShareFileRequest;
using rpc::ShareFileResponse;

// 同步分享逻辑，返回 HTTP 响应码（0=OK，1=FAIL，2=ALREADY_SHARED）
static int SyncShareFile(const std::string &user,
                         const std::string &md5,
                         const std::string &filename)
{
    // 1. 拼 fileid
    std::string fileid = md5 + filename;

    // 2. Redis 检查是否已分享
    auto &redis = get_redis();
    if (redis.zscore(FILE_PUBLIC_ZSET, fileid).has_value())
    {
        // Redis 有记录：已分享
        return 3;
    }

    // 3. 再查 MySQL share_file_list
    std::string selShare =
        "SELECT 1 FROM share_file_list "
        "WHERE md5='" +
        md5 +
        "' AND file_name='" + filename + "'";
    auto r0 = SakilaDatabase.Query(selShare.c_str());
    if (r0 && r0->GetRowCount() > 0)
    {
        // MySQL 有记录，但 Redis 未同步——补 Redis 后中断
        redis.zadd(FILE_PUBLIC_ZSET, fileid, 0.0);
        redis.hset(FILE_NAME_HASH, fileid, filename);
        return 3;
    }

    // 4. 第一次分享：写 user_file_list.shared_status
    try
    {
        std::string updStatus =
            "UPDATE user_file_list SET shared_status=1 "
            "WHERE user='" +
            user +
            "' AND md5='" + md5 +
            "' AND file_name='" + filename + "'";
        SakilaDatabase.Execute(updStatus.c_str());
    }
    catch (...)
    {
        return 1; // 1
    }

    // 5. 插入 share_file_list
    std::string timestr = _now_str();
    try
    {
        std::string insShare =
            "INSERT INTO share_file_list "
            "(user, md5, create_time, file_name, pv) VALUES ('" +
            user + "','" + md5 + "','" + timestr + "','" + filename + "',0)";
        SakilaDatabase.Execute(insShare.c_str());
    }
    catch (...)
    {
        return 1; // 1
    }

    // 6. 维护 user_file_count
    try
    {
        // 6a. 查 count
        std::string selCnt =
            "SELECT count FROM user_file_count "
            "WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'";
        auto r1 = SakilaDatabase.Query(selCnt.c_str());
        if (!r1 || r1->GetRowCount() == 0)
        {
            // 插入新行
            std::string insCnt =
                "INSERT INTO user_file_count "
                "(user, count) VALUES('xxx_share_xxx_file_xxx_list_xxx_count_xxx',1)";
            SakilaDatabase.Execute(insCnt.c_str());
        }
        else
        {
            int cnt = r1->Fetch()[0].GetInt32() + 1;
            std::string updCnt =
                "UPDATE user_file_count SET count=" + std::to_string(cnt) +
                " WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'";
            SakilaDatabase.Execute(updCnt.c_str());
        }
    }
    catch (...)
    {
        return 1; // 1
    }

    // 7. Redis 同步：zadd + hset
    redis.zadd(FILE_PUBLIC_ZSET, fileid, 0.0);
    redis.hset(FILE_NAME_HASH, fileid, filename);

    return 0;
}

template <>
struct ServiceMethodTraits<ShareFileRequest>
{
    using ResponseType = ShareFileResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestShareFile;
};
class ShareFileCall
    : public CallData<ShareFileRequest, ShareFileCall>
{
public:
    ShareFileCall(rpc::DatabaseService::AsyncService *svc,
                  grpc::ServerCompletionQueue *cq)
        : CallData<ShareFileRequest, ShareFileCall>(svc, cq) {}
    void OnRequest(const ShareFileRequest &req,
                   ShareFileResponse &reply)
    {
        int code = SyncShareFile(req.user(), req.md5(), req.filename());
        reply.set_code(code);
        rpc_finish();
    }
};