// DeleteFileCall.h
#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include <string>

// RPC 返回码
static constexpr int HTTP_RESP_OK             = 0;
static constexpr int HTTP_RESP_FAIL           = 1;
static constexpr int HTTP_RESP_USER_EXIST     = 2;
static constexpr int HTTP_RESP_DEALFILE_EXIST = 3;
static constexpr int HTTP_RESP_TOKEN_ERR      = 4;
static constexpr int HTTP_RESP_FILE_EXIST     = 5;

using rpc::DeleteFileRequest;
using rpc::DeleteFileResponse;

template <>
struct ServiceMethodTraits<DeleteFileRequest> {
    using ResponseType = DeleteFileResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestDeleteFile;
};

class DeleteFileCall
    : public CallData<DeleteFileRequest, DeleteFileCall> {
public:
    DeleteFileCall(rpc::DatabaseService::AsyncService* svc,
                   grpc::ServerCompletionQueue* cq)
        : CallData<DeleteFileRequest, DeleteFileCall>(svc, cq) {}

    void OnRequest(const DeleteFileRequest& req,
                   DeleteFileResponse& reply) {
        // 调用同步删除逻辑，获取 code 和（可选的）file_id
        std::string file_id;
        int code = SyncDeleteFile(req.user(), req.md5(), req.filename(), file_id);

        // 填充响应
        reply.set_code(code);
        if (code == 2 && !file_id.empty()) {
            reply.set_file_id(file_id);
        }

        // 完成 RPC
        rpc_finish();
    }

private:
    // 同步删除逻辑，输出 file_id 用于物理删除场景
    int SyncDeleteFile(const std::string& user,
                       const std::string& md5,
                       const std::string& filename,
                       std::string& out_file_id) {
        // 1. 拼 fileid
        std::string fileid = md5 + filename;

        // 2. Redis 检查分享集合
        auto& redis = get_redis();
        bool is_shared        = false;
        bool redis_has_record = false;
        if (redis.zscore(FILE_PUBLIC_ZSET, fileid).has_value()) {
            is_shared        = true;
            redis_has_record = true;
        } else {
            // 再查 MySQL shared_status
            std::string sql0 =
                "SELECT shared_status FROM user_file_list "
                "WHERE user='" + user +
                "' AND md5='" + md5 +
                "' AND file_name='" + filename + "'";
            auto r0 = SakilaDatabase.Query(sql0.c_str());
            if (!r0 || r0->GetRowCount() == 0) {
                return HTTP_RESP_FAIL;
            }
            int shared_status = r0->Fetch()[0].GetInt32();
            if (shared_status == 1) {
                is_shared = true;
            }
        }

        // 3. 已分享：删 share_file_list + 更新 share count + 同步 Redis
        if (is_shared) {
            try {
                std::string delShare =
                    "DELETE FROM share_file_list "
                    "WHERE user='" + user +
                    "' AND md5='" + md5 +
                    "' AND file_name='" + filename + "'";
                SakilaDatabase.Execute(delShare.c_str());

                std::string selShareCnt =
                    "SELECT count FROM user_file_count "
                    "WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'";
                auto r1 = SakilaDatabase.Query(selShareCnt.c_str());
                if (!r1 || r1->GetRowCount() == 0)
                    return HTTP_RESP_FAIL;

                int shareCnt = r1->Fetch()[0].GetInt32();
                shareCnt = std::max(0, shareCnt - 1);
                std::string updShareCnt =
                    "UPDATE user_file_count SET count=" +
                    std::to_string(shareCnt) +
                    " WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'";
                SakilaDatabase.Execute(updShareCnt.c_str());

            } catch (...) {
                return HTTP_RESP_FAIL;
            }

            if (redis_has_record) {
                redis.zrem(FILE_PUBLIC_ZSET, fileid);
                redis.hdel(FILE_NAME_HASH,   fileid);
            }
        }

        // 4. 用户文件总数减 1
        {
            std::string selUserCnt =
                "SELECT count FROM user_file_count "
                "WHERE user='" + user + "'";
            auto r2 = SakilaDatabase.Query(selUserCnt.c_str());
            if (!r2 || r2->GetRowCount() == 0)
                return HTTP_RESP_FAIL;

            int userCnt = r2->Fetch()[0].GetInt32();
            if (userCnt >= 1) {
                userCnt--;
                try {
                    std::string updUserCnt =
                        "UPDATE user_file_count SET count=" +
                        std::to_string(userCnt) +
                        " WHERE user='" + user + "'";
                    SakilaDatabase.Execute(updUserCnt.c_str());
                } catch (...) {
                    return HTTP_RESP_FAIL;
                }
            }
        }

        // 5. 从 user_file_list 删除记录
        {
            try {
                std::string delUserList =
                    "DELETE FROM user_file_list "
                    "WHERE user='" + user +
                    "' AND md5='" + md5 +
                    "' AND file_name='" + filename + "'";
                SakilaDatabase.Execute(delUserList.c_str());
            } catch (...) {
                return HTTP_RESP_FAIL;
            }
        }

        // 6. 处理 file_info 引用计数，可能物理删除
        {
            std::string selFileInfo =
                "SELECT count, file_id FROM file_info "
                "WHERE md5='" + md5 + "'";
            auto r3 = SakilaDatabase.Query(selFileInfo.c_str());
            if (!r3 || r3->GetRowCount() == 0)
                return HTTP_RESP_FAIL;

            int refCnt      = r3->Fetch()[0].GetInt32();
            out_file_id     = r3->Fetch()[1].GetString();

            if (refCnt > 1) {
                refCnt--;
                try {
                    std::string updRef =
                        "UPDATE file_info SET count=" +
                        std::to_string(refCnt) +
                        " WHERE md5='" + md5 + "'";
                    SakilaDatabase.Execute(updRef.c_str());
                } catch (...) {
                    return HTTP_RESP_FAIL;
                }
            } else {
                // 元数据删除
                try {
                    std::string delFileInfo =
                        "DELETE FROM file_info WHERE md5='" + md5 + "'";
                    SakilaDatabase.Execute(delFileInfo.c_str());
                } catch (...) {
                    return HTTP_RESP_FAIL;
                }
                // 物理删除
                return 2;  // 物理删除
            }
        }

        return HTTP_RESP_OK;
    }
}; 






// // DeleteFileCall.h
// #pragma once

// #include <grpcpp/grpcpp.h>
// #include "mysql_rpc.grpc.pb.h"
// #include "RedisClient.h"
// #include "RpcCalldata.h"
// #include "Implementation/SakilaDatabase.h"
// #include "MySQLPreparedStatement.h"
// #include <string>

// // RPC 返回码
// static constexpr int HTTP_RESP_OK             = 0;
// static constexpr int HTTP_RESP_FAIL           = 1;
// static constexpr int HTTP_RESP_USER_EXIST     = 2;
// static constexpr int HTTP_RESP_DEALFILE_EXIST = 3;
// static constexpr int HTTP_RESP_TOKEN_ERR      = 4;
// static constexpr int HTTP_RESP_FILE_EXIST     = 5;

// using rpc::DeleteFileRequest;
// using rpc::DeleteFileResponse;

// template <>
// struct ServiceMethodTraits<DeleteFileRequest> {
//     using ResponseType = DeleteFileResponse;
//     static constexpr auto Method =
//         &rpc::DatabaseService::AsyncService::RequestDeleteFile;
// };

// class DeleteFileCall
//     : public CallData<DeleteFileRequest, DeleteFileCall> {
// public:
//     DeleteFileCall(rpc::DatabaseService::AsyncService* svc,
//                    grpc::ServerCompletionQueue* cq)
//       : CallData<DeleteFileRequest, DeleteFileCall>(svc, cq)
//     {}

//     void OnRequest(const DeleteFileRequest& req,
//                    DeleteFileResponse&      reply) 
//     {
//         std::string file_id;
//         int code = SyncDeleteFile(req.user(),
//                                   req.md5(),
//                                   req.filename(),
//                                   file_id);

//         reply.set_code(code);
//         if (code == HTTP_RESP_DEALFILE_EXIST && !file_id.empty()) {
//             reply.set_file_id(file_id);
//         }
//         rpc_finish();
//     }

// private:
//     int SyncDeleteFile(const std::string& user,
//                        const std::string& md5,
//                        const std::string& filename,
//                        std::string&       out_file_id)
//     {
//         auto& db    = SakilaDatabase;
//         auto& redis = get_redis();
//         bool is_shared  = false;
//         bool redis_has  = false;
//         const std::string fileid = md5 + filename;

//         // 1) Redis 检查
//         if (redis.zscore(FILE_PUBLIC_ZSET, fileid).has_value()) {
//             is_shared = redis_has = true;
//         }
//         else {
//             // 1a) MySQL 查询 shared_status
//             auto st0 = db.GetPreparedStatement(CHECK_USER_FILE_LIST_STATUS_SYNC);
//             st0->setString(0, user);
//             st0->setString(1, md5);
//             st0->setString(2, filename);
//             auto rs0 = SakilaDatabase.Query(st0);
//             if (!rs0 || rs0->GetRowCount() == 0) {
//                 return HTTP_RESP_FAIL;
//             }
//             int shared_status = rs0->Fetch()[0].GetInt32();
//             if (shared_status == 1) is_shared = true;
//         }

//         // 2) 已分享：删 share_file_list + 更新 share count + 同步 Redis
//         if (is_shared) {
//             // DELETE_SHARE_FILE_SYNC
//             {
//                 auto st = db.GetPreparedStatement(DELETE_SHARE_FILE_SYNC);
//                 st->setString(0, user);
//                 st->setString(1, md5);
//                 st->setString(2, filename);
//                 SakilaDatabase.Execute(st);
//             }
//             // GET_SHARE_FILE_COUNT_SYNC
//             int shareCnt = 0;
//             {
//                 auto st = db.GetPreparedStatement(GET_SHARE_FILE_COUNT_SYNC);
//                 st->setString(0, user);
//                 auto rs = SakilaDatabase.Query(st);
//                 shareCnt = rs->Fetch()[0].GetInt32();
//             }
//             // UPDATE_SHARE_FILE_COUNT_SYNC
//             shareCnt = std::max(0, shareCnt - 1);
//             {
//                 auto st = db.GetPreparedStatement(UPDATE_SHARE_FILE_COUNT_SYNC);
//                 st->setInt32(0, shareCnt);
//                 st->setString(1, user);
//                 SakilaDatabase.Execute(st);
//             }
//             if (redis_has) {
//                 redis.zrem(FILE_PUBLIC_ZSET, fileid);
//                 redis.hdel(FILE_NAME_HASH,   fileid);
//             }
//         }

//         // 3) 用户文件总数减 1
//         {
//             auto st = db.GetPreparedStatement(GET_USER_FILE_COUNT_SYNC);
//             st->setString(0, user);
//             auto rs = SakilaDatabase.Query(st);
//             int userCnt = rs->Fetch()[0].GetInt32();
//             if (userCnt > 0) {
//                 auto stu = db.GetPreparedStatement(UPDATE_USER_FILE_COUNT_SYNC);
//                 stu->setInt32(0, userCnt - 1);
//                 stu->setString(1, user);
//                 SakilaDatabase.Execute(stu);
//             }
//         }

//         // 4) 删除 user_file_list
//         {
//             auto st = db.GetPreparedStatement(DELETE_USER_FILE_LIST_SYNC);
//             st->setString(0, user);
//             st->setString(1, md5);
//             st->setString(2, filename);
//             SakilaDatabase.Execute(st);
//         }

//         // 5) 处理 file_info 引用计数
//         int refCnt = 0;
//         {
//             auto st = db.GetPreparedStatement(CHECK_FILE_INFO_COUNT_SYNC);
//             st->setString(0, md5);
//             auto rs=SakilaDatabase.Query(st);
//             refCnt = rs->Fetch()[0].GetInt32();
//         }
//         {
//             auto st = db.GetPreparedStatement(GET_FILE_ID_SYNC);
//             st->setString(0, md5);
//             auto rs = SakilaDatabase.Query(st);
//             out_file_id = rs->Fetch()[0].GetString();
//         }

//         if (refCnt > 1) {
//             // UPDATE_FILE_INFO_COUNT_SYNC
//             auto st = db.GetPreparedStatement(UPDATE_FILE_INFO_COUNT_SYNC);
//             st->setInt32(0, refCnt - 1);
//             st->setString(1, md5);
//             SakilaDatabase.Execute(st);
//             return HTTP_RESP_OK;
//         }
//         else {
//             // DELETE_FILE_INFO_SYNC
//             auto st = db.GetPreparedStatement(DELETE_FILE_INFO_SYNC);
//             st->setString(0, md5);
//             SakilaDatabase.Execute(st);
//             return 2;
//         }
//     }
// };
