#ifndef SERVER_IMPL_H
#define SERVER_IMPL_H

#include <memory>
#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "Global.h"
#include "RpcCalldata.h"
#include "dbapi/commom.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using rpc::DatabaseService;


//当该服务加入新方法时，需要哦在析构种加入该方法线程的析构
//Run()函数加入对应的初始handle创建

class RpcServer
{
public:
    ~RpcServer()
    {
        // 安全释放所有线程
        server_->Shutdown();
        cq_->Shutdown();
        RegisterUserCall::processor_stop_flag_.store(true);
        LoginUserCall::processor_stop_flag_.store(true);
        InstantUploadCall::processor_stop_flag_.store(true);
        if (RegisterUserCall::processor_thread_.joinable())
            RegisterUserCall::processor_thread_.join();
        if (LoginUserCall::processor_thread_.joinable())
            LoginUserCall::processor_thread_.join();
        if (InstantUploadCall::processor_thread_.joinable())
            InstantUploadCall::processor_thread_.join();
    }

    void Run()
    {
        ServerBuilder builder;
        builder.AddListeningPort(
            // Global::Instance().get<std::string>("Mysql_Rpc_Server"),
            "0.0.0.0:50051",
            grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        std::cout << "gRPC server listening on "
                  << Global::Instance().get<std::string>("Mysql_Rpc_Server")
                  << std::endl;

        // 为每个 RPC spawn 第一个 handler
        new LoginUserCall(&service_, cq_.get());
        new RegisterUserCall(&service_, cq_.get());
        new InstantUploadCall(&service_, cq_.get());

        HandleRpcs();
    }

private:
    void HandleRpcs()
    {
        void *tag;
        bool ok;
        while (cq_->Next(&tag, &ok))
        {
            GPR_ASSERT(ok);
            // tag 就是我们 new 出来的 CallData 对象
            static_cast<CallDataBase *>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    DatabaseService::AsyncService service_;
    std::unique_ptr<Server> server_;
};

#endif // SERVER_IMPL_H
