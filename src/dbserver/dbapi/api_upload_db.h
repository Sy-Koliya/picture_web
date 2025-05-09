#ifndef API_UPLOAD_FILE_DB_H
#define API_UPLOAD_FILE_DB_H

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "AsyncCallbackProcessor.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include "QueryHolder.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

using rpc::UploadRequest;
using rpc::UploadResponse;

template<>
struct ServiceMethodTraits<UploadRequest> {
    using ResponseType = UploadResponse;
    static constexpr auto Method =
        &rpc::DatabaseService::AsyncService::RequestUploadFile; 
};

class UploadFileCall : public CallData<UploadRequest, UploadFileCall> {
public:
    using Request  = UploadRequest;
    using Response = UploadResponse;
    using Base     = CallData<Request, UploadFileCall>;

    UploadFileCall(rpc::DatabaseService::AsyncService* service,
                   grpc::ServerCompletionQueue* cq)
      : Base(service, cq)
    {
        std::call_once(processor_init_flag_, [](){
            processor_thread_ = std::thread([]{
                while (!processor_stop_flag_.load()) {
                    processor_.ProcessReadyCallbacks();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        });
    }

    void OnRequest(const Request& req, Response& /*unused*/) {
        // 1. 插入 file_info
        auto stmt1 = SakilaDatabase.GetPreparedStatement(INSERT_FILE_INFO);
        stmt1->setString (0, req.file_md5());
        stmt1->setString (1, req.fileid());
        stmt1->setString (2, req.url());
        stmt1->setInt64  (3, req.file_size());
        stmt1->setString (4, GetSuffix(req.filename()));  
        stmt1->setUInt32 (5, 1);

        // 2. 链式调用：先插入 file_info，再插入 user_file_list
        auto chain = SakilaDatabase
          .AsyncQuery(stmt1)
          .WithChainingPreparedCallback(
            [this, req]
            (QueryCallback& cb, PreparedQueryResult /*res1*/) {
                // 插入 user_file_list
                auto stmt2 = SakilaDatabase.GetPreparedStatement(INSERT_USER_FILE_LIST);
                stmt2->setString(0, req.user());
                stmt2->setString(1, req.file_md5());
                stmt2->setString(2, now_str());
                stmt2->setString(3, req.filename());
                stmt2->setUInt32(4, 0);
                stmt2->setUInt32(5, 0);
                cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmt2));
          })
          // 3. 查询 user_file_count
          .WithChainingPreparedCallback(
            [this, user = req.user()](QueryCallback& cb, PreparedQueryResult /*res2*/) {
                auto stmt3 = SakilaDatabase.GetPreparedStatement(SELECT_USER_FILE_COUNT);
                stmt3->setString(0, user);
                cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmt3));
          })
          // 4. 插入或更新 user_file_count
          .WithChainingPreparedCallback(
            [this, user = req.user()](QueryCallback& cb, PreparedQueryResult res3) {
                if (!res3 || res3->GetRowCount() == 0) {
                    // 首次插入
                    auto stmt4 = SakilaDatabase.GetPreparedStatement(INSERT_USER_FILE_COUNT);
                    stmt4->setString(0, user);
                    stmt4->setUInt32(1, 1);
                    cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmt4));
                } else {
                    // 更新计数
                    uint32_t cnt = res3->Fetch()[0].GetUInt32() + 1;
                    auto stmt5 = SakilaDatabase.GetPreparedStatement(UPDATE_USER_FILE_COUNT);
                    stmt5->setUInt32(0, cnt);
                    stmt5->setString(1, user);
                    cb.SetNextQuery(SakilaDatabase.AsyncQuery(stmt5));
                }
          })
          // 5. 最终回调：返回结果
          .WithChainingPreparedCallback(
            [this](QueryCallback&, PreparedQueryResult) {
                this->reply_.set_code(0);
                this->status_ = FINISH;
                this->responder_.Finish(this->reply_, grpc::Status::OK, this);
          });

        processor_.AddCallback(std::move(chain));
    }

private:
    static std::string now_str() {
        char buf[64];
        auto t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&t));
        return buf;
    }
    static std::string GetSuffix(const std::string& name) {
        auto pos = name.rfind('.');
        return pos == std::string::npos ? "" : name.substr(pos + 1);
    }

    friend class MySqlRpcServer;
    inline static AsyncCallbackProcessor<QueryCallback> processor_;
    inline static std::once_flag                  processor_init_flag_;
    inline static std::thread                      processor_thread_;
    inline static std::atomic<bool>                processor_stop_flag_{false};
};

#endif // API_UPLOAD_FILE_DB_H