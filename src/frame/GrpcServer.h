

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include "mysql_rpc.grpc.pb.h"
#include "Global.h"
#include "dbapi/api_register_db.h"


using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;


// int main(int argc, char **argv)
// {
//     ServerImpl server;
//     server.Run();

//     return 0;
// }


// server_impl.h
#ifndef SERVER_IMPL_H
#define SERVER_IMPL_H

#include <memory>
#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "Global.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;

using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;

struct CallDataBase {
    virtual void Proceed() = 0;
    virtual ~CallDataBase() = default;
};


template<
    typename Req,
    typename Resp,
    typename Derived,
    typename ServiceType,
    void (ServiceType::*RequestMethod)(
        grpc::ServerContext*,
        Req*,
        grpc::ServerAsyncResponseWriter<Resp>*,
        grpc::CompletionQueue*,
        grpc::ServerCompletionQueue*,
        void*
    )
>
class CallData 
  : public CallDataBase
{
public:
    CallData(DatabaseService::AsyncService* service,
             ServerCompletionQueue* cq)
      : service_(service)
      , cq_(cq)
      , responder_(&ctx_)
      , status_(CREATE)
    {
        // 一上来就注册一个 CREATE 事件
        Proceed();
    }

    void Proceed() override {
        if (status_ == CREATE) {

            status_ = PROCESS;
            //注册事件到compeletequeue
            (service_->*RequestMethod)(
                &ctx_,
                &request_,
                &responder_,
                cq_, cq_,
                this  // tag
            );

        } else if (status_ == PROCESS) {
            // 收到客户端请求，先 spawn 下一个 handler
            new Derived(service_, cq_);
            // 然后执行用户业务
            static_cast<Derived*>(this)->OnRequest(request_, reply_);
            // 回复，并回到 cq_ 等待 FINISH

        } else {
            // FINISH：清理自己
            delete this;
        }
    }

    void OnRequest(){Req req, Resp rsp}{
        //异步调用结尾得有这两句
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);
    }

private:

    DatabaseService::AsyncService*        service_;
    ServerCompletionQueue*                cq_;
    ServerContext                         ctx_;
    Req                                   request_;
    Resp                                  reply_;
    ServerAsyncResponseWriter<Resp>       responder_;
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus                            status_;
};




// 如果还有别的 RPC，比如 Login，再写一个 LoginCall 即可：
// class LoginCall : public CallData<
//      LoginRequest,
//      LoginResponse,
//      LoginCall,
//      &DatabaseService::AsyncService::Requestlogin
// > {
// public:
//     using Base = CallData<
//         LoginRequest,
//         LoginResponse,
//         LoginCall,
//         &DatabaseService::AsyncService::Requestlogin
//     >;
//     LoginCall(DatabaseService::AsyncService* s, ServerCompletionQueue* cq)
//       : Base(s, cq) {}
//     void OnRequest(const LoginRequest& req, LoginResponse& resp) {
//         // …
//     }
// };

// —— 4) 最终的 ServerImpl —— 
class ServerImpl {
public:
    ~ServerImpl() {
        server_->Shutdown();
        cq_->Shutdown();
    }

    void Run() {
        ServerBuilder builder;
        builder.AddListeningPort(
            Global::Instance().get<std::string>("Mysql_Rpc_Server"),
            grpc::InsecureServerCredentials()
        );
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        std::cout << "gRPC server listening on "
                  << Global::Instance().get<std::string>("Mysql_Rpc_Server")
                  << std::endl;

        // 为每个 RPC spawn 第一个 handler
        new RegisterUserCall(&service_, cq_.get());
        // new LoginCall(&service_, cq_.get());
        // …如果有更多 RPC 就继续 new 它们…

        HandleRpcs();
    }

private:
    void HandleRpcs() {
        void* tag;
        bool ok;
        while (cq_->Next(&tag, &ok)) {
            GPR_ASSERT(ok);
            // tag 就是我们 new 出来的 CallData 对象
            static_cast<CallDataBase*>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    DatabaseService::AsyncService      service_;
    std::unique_ptr<Server>           server_;
};




#endif // SERVER_IMPL_H
