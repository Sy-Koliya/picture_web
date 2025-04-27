#ifndef SERVER_IMPL_H
#define SERVER_IMPL_H

#include <memory>
#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "Global.h"
#include "dbapi/api_register_db.h"
#include "RpcCalldata.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;

using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;



class RegisterUserCall;
class RpcServer {
public:
    ~RpcServer() {
        server_->Shutdown();
        cq_->Shutdown();
        RegisterUserCall::processor_stop_flag_.store(true);
        if (RegisterUserCall::processor_thread_.joinable())
           RegisterUserCall::processor_thread_.join();
    }

    void Run() {
        ServerBuilder builder;
        builder.AddListeningPort(
            //Global::Instance().get<std::string>("Mysql_Rpc_Server"),
            "localhost:50051",
            grpc::InsecureServerCredentials()
        );
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        std::cout << "gRPC server listening on "
                  << Global::Instance().get<std::string>("Mysql_Rpc_Server")
                  << std::endl;

        // 为每个 RPC spawn 第一个 handler
        // new LoginCall(&service_, cq_.get());
        new RegisterUserCall(&service_, cq_.get());
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
