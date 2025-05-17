// DeleteFileCall.h
#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "AsyncCallbackProcessor.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include "QueryHolder.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

using rpc::DeleteFileRequest;
using rpc::DeleteFileResponse;

template <>
struct ServiceMethodTraits<rpc::DeleteFileRequest>
{
    using ResponseType = rpc::DeleteFileResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestDeleteFile;
};

class DeleteFileCall
    : public CallData<DeleteFileRequest, DeleteFileCall>
{
public:
    using Request = DeleteFileRequest;
    using Response = DeleteFileResponse;
    using Base = CallData<Request, DeleteFileCall>;

    DeleteFileCall(rpc::DatabaseService::AsyncService *svc,
                   grpc::ServerCompletionQueue *cq)
        : Base(svc, cq)
    {
        std::call_once(processor_init_flag_, []
                       { processor_thread_ = std::thread([]
                                                         {
                while (!processor_stop_flag_.load()) {
                    processor_.ProcessReadyCallbacks();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } }); });
    }

    void OnRequest(const DeleteFileRequest& req,
                   DeleteFileResponse& reply) 
    {
        const std::string fileid = req.md5() + req.filename();
        auto &redis = get_redis();

        if (redis.zscore(FILE_PUBLIC_ZSET, fileid).has_value()) {
              BuildShareChain(req, reply);
        } else {
            // redis 未命中，先查 MySQL 再分支
            auto stm=SakilaDatabase.GetPreparedStatement(CHECK_SHARE_FILE_EXIST);
                           stm->setString(0, req.md5());
                           stm->setString(1, req.filename());
            auto work = SakilaDatabase.AsyncQuery(stm)
                   .WithChainingPreparedCallback(
                     [&](QueryCallback &cb, PreparedQueryResult r1) {
                       if (!r1) {
                        BuildUserChain(req, reply);
                      } 
                       else if (r1->GetRowCount()>0)
                        BuildShareChain(req, reply);
                       else{
                        std::cout<<"error"<<'\n';
                       }
                   });
                
        processor_.AddCallback(std::move(work));
        }
    }

private:
    // 删除分享并更新 count，最后跳入用户清理链
    void BuildShareChain(const DeleteFileRequest& req,
                                        DeleteFileResponse& reply)
    {
        auto stmtDel = SakilaDatabase.GetPreparedStatement(DELETE_SHARE_FILE);
        stmtDel->setString(0, req.user());
        stmtDel->setString(1, req.md5());
        stmtDel->setString(2, req.filename());

        auto work= SakilaDatabase.AsyncQuery(stmtDel)
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r2) {
            if (!r2) { reply.set_code(1); rpc_finish(); return; }
            auto stmtGet = SakilaDatabase.GetPreparedStatement(GET_SHARE_FILE_COUNT);
            stmtGet->setString(0, req.user());
            cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtGet));
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r3) {
            if (!r3) { reply.set_code(1); rpc_finish(); return; }
            int cnt = r3->Fetch()[0].GetInt32();
            auto stmtUpd = SakilaDatabase.GetPreparedStatement(UPDATE_SHARE_FILE_COUNT);
            stmtUpd->setInt32(0, std::max(0, cnt-1));
            stmtUpd->setString(1, req.user());
            cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtUpd));
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r4) {
            if (!r4) { reply.set_code(1); rpc_finish(); }
            else    BuildUserChain(req, reply);
        });
        processor_.AddCallback(std::move(work));
    }

    // 用户清理：减 count → 删列表 → 处理 file_info → finish
    void BuildUserChain(const DeleteFileRequest& req,
                                       DeleteFileResponse& reply)
    {
        auto stmtGetUc = SakilaDatabase.GetPreparedStatement(GET_USER_FILE_COUNT);
        stmtGetUc->setString(0, req.user());
        
        auto work=  SakilaDatabase.AsyncQuery(stmtGetUc)
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r5) {
            if (!r5) { reply.set_code(1); rpc_finish(); return; }
            int ucnt = r5->Fetch()[0].GetInt32();
            if (ucnt > 0) {
                auto stmtUpdUc = SakilaDatabase.GetPreparedStatement(UPDATE_USER_FILE_COUNT);
                stmtUpdUc->setInt32(0, ucnt-1);
                 stmtUpdUc->setString(1, req.user());
                cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtUpdUc));
            }else{
                reply.set_code(1); rpc_finish(); return;
            }
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r6) {
            auto stmtDelUf = SakilaDatabase.GetPreparedStatement(DELETE_USER_FILE_LIST);
            stmtDelUf->setString(0, req.user());
            stmtDelUf->setString(1, req.md5());
            stmtDelUf->setString(2, req.filename());
            cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtDelUf));
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r7) {
            auto stmtRef = SakilaDatabase.GetPreparedStatement(GET_FILE_REF_COUNT);
            stmtRef->setString(0, req.md5());
            cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtRef));
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback&, PreparedQueryResult r8) {
            if (!r8) {
                reply.set_code(1);
                rpc_finish();
            } else {
                int ref = r8->Fetch()[0].GetInt32();
                if (ref > 1) {
                    auto stmtUpdRef = SakilaDatabase.GetPreparedStatement(UPDATE_FILE_REF_COUNT);
                    stmtUpdRef->setInt32(0, ref-1);
                    stmtUpdRef->setString(1, req.md5());;
                    SakilaDatabase.AsyncQuery(stmtUpdRef);  // fire-and-forget
                    reply.set_code(0);
                    rpc_finish();
                } else {
                   RemoveFile(req,reply);
                }
            }
        });
        processor_.AddCallback(std::move(work));
    }
  
     void RemoveFile(const DeleteFileRequest& req,
                           DeleteFileResponse& reply){
        auto stmtFileid = SakilaDatabase.GetPreparedStatement(GET_USER_FILE_ID);;
        stmtFileid->setString(0, req.md5());
        auto work= SakilaDatabase.AsyncQuery(stmtFileid)
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r2) {
            if (!r2) { reply.set_code(1); rpc_finish(); return; }
            reply.set_file_id(r2->Fetch()[0].GetString());
            auto stmtDelFile = SakilaDatabase.GetPreparedStatement(DELETE_FILE_INFO);
            stmtDelFile->setString(0, req.md5());
            cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmtDelFile));
        })
        .WithChainingPreparedCallback(
          [&](QueryCallback &cb, PreparedQueryResult r3) {
            reply.set_code(2);
            this->rpc_finish();
        });
        processor_.AddCallback(std::move(work));
     }
private:
    friend class MySqlRpcServer;

    inline static AsyncCallbackProcessor<QueryCallback> processor_;
    inline static std::once_flag processor_init_flag_;
    inline static std::thread processor_thread_;
    inline static std::atomic<bool> processor_stop_flag_{false};
};
