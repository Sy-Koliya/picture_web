#ifndef API_LOGIN_DB_H
#define API_LOGIN_DB_H

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

using grpc::Status;

// 类型萃取特化（必须在服务定义之后）
template<>
struct ServiceMethodTraits<rpc::LoginRequest> {
    using ResponseType = rpc::LoginResponse;
    static constexpr auto Method = 
        &rpc::DatabaseService::AsyncService::RequestloginUser;
};

class LoginUserCall : public CallData<rpc::LoginRequest, LoginUserCall> {
public:
    using Request  = rpc::LoginRequest;
    using Response = rpc::LoginResponse;
    using Base     = CallData<Request, LoginUserCall>;

    LoginUserCall(rpc::DatabaseService::AsyncService* service,
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

    void OnRequest(const Request& req, Response& /*resp*/) 
    {
        auto stmt = SakilaDatabase.GetPreparedStatement(
                        SakilaDatabaseStatements::CHECK_LOGIN_PASSWORD
                    );
        stmt->setString(0, req.user_name());

        auto qc = SakilaDatabase
          .AsyncQuery(stmt)
          .WithChainingPreparedCallback(
            [this, pwd = req.password()](QueryCallback& cb, PreparedQueryResult res)
            {
                bool ok = (res && res->GetRowCount() > 0 && 
                            res->Fetch()[0].GetString() == pwd);
                this->reply_.set_code(ok ? 0 : 1); // 0=成功 1=失败
                this->status_ = FINISH;
                this->responder_.Finish(reply_, Status::OK, this);
            });

        processor_.AddCallback(std::move(qc));
    }

private:
    friend class MySqlRpcServer;
    inline static AsyncCallbackProcessor<QueryCallback> processor_;
    inline static std::once_flag processor_init_flag_;
    inline static std::thread processor_thread_;
    inline static std::atomic<bool> processor_stop_flag_{false};
};

#endif // API_LOGIN_DB_H