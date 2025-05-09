#ifndef API_REGISTER_DB_H

#define API_REGISTER_DB_H

// server.cpp
#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "mysql_rpc.pb.h"
#include "AsyncCallbackProcessor.h"
#include "RpcCalldata.h"
// dbimp
#include "DatabaseEnvFwd.h"
#include "Log.h"

#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Implementation/SakilaDatabase.h"
#include "MySQLThreading.h"
#include "AsyncCallbackProcessor.h"
#include "QueryHolder.h"

#include <mutex>
#include <thread>
#include <chrono>



template<>
struct ServiceMethodTraits<rpc::RegisterRequest> {
    using ResponseType = rpc::RegisterResponse;
    static constexpr auto Method = 
        &rpc::DatabaseService::AsyncService::RequestregisterUser;
};

class RegisterUserCall : public CallData<rpc::RegisterRequest, RegisterUserCall> {
  public:
      using Request  = rpc::RegisterRequest;
      using Response = rpc::RegisterResponse;
      using Base     = CallData<Request, RegisterUserCall>;

    // 构造时启动一次全局后台线程（第一次调用）
    RegisterUserCall(rpc::DatabaseService::AsyncService* service,
                     grpc::ServerCompletionQueue* cq)
      : Base(service, cq)
    {
        std::call_once(processor_init_flag_, [](){
             // jthread 的析构会自动 request_stop() 并 join()
             processor_thread_ = std::thread([]{
                while (!processor_stop_flag_.load()) {
                  processor_.ProcessReadyCallbacks();
                  std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            });
        });
    }

    // 收到请求时调用：只负责投递 callback，到后台线程处理
    void OnRequest(const Request& req, Response& resp)
    {
        auto check_exist_stmt = SakilaDatabase.GetPreparedStatement(CHECK_REGISTER_INFO_EXIST);
        check_exist_stmt->setString(0,req.user_name());
        auto* insert_info_stmt = SakilaDatabase.GetPreparedStatement(REGISTER_INTO_USER_INFO);
        insert_info_stmt->setString(0, req.user_name());
        insert_info_stmt->setString(1,req.nick_name());
        insert_info_stmt->setString(2,req.password());
        insert_info_stmt->setString(3,req.phone());
        insert_info_stmt->setString(4,req.email());
        

        auto qc = SakilaDatabase
        .AsyncQuery(check_exist_stmt)
      
        .WithChainingPreparedCallback(
          [this,insert_info_stmt](QueryCallback& cb, PreparedQueryResult res)
          {
            bool userExists = res && res->GetRowCount() > 0;
            if(userExists)
            {
              this->reply_.set_code(2);
              this->status_ = FINISH;
              this->responder_.Finish(reply_, Status::OK, this);
            }
            else
            {
              cb.SetNextQuery(
                SakilaDatabase.AsyncQuery(insert_info_stmt)
              );
            }
          })
      
        .WithChainingPreparedCallback(
          [this](QueryCallback& cb, PreparedQueryResult res)
          {
            this->reply_.set_code(0);
            this->status_ = FINISH;
            this->responder_.Finish(reply_, Status::OK, this);
          });

      processor_.AddCallback(std::move(qc));
      
    }

private:
    // 全局唯一的 AsyncCallbackProcessor
    friend class MySqlRpcServer;
    inline static AsyncCallbackProcessor<QueryCallback> processor_;

    // 确保线程只启动一次
    inline static std::once_flag processor_init_flag_;
    inline static std::thread          processor_thread_;
    inline static std::atomic<bool>    processor_stop_flag_{false};
};


#endif