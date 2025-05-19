#ifndef SERVER_IMPL_H
#define SERVER_IMPL_H

#include <memory>
#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>
#include "Global.h"
#include "RpcCalldata.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using rpc::DatabaseService;

// 当该服务加入新方法时，需要哦在析构种加入该方法线程的析构
// Run()函数加入对应的初始handle创建

template <typename T>
class RpcServer {
public:
    void Run(const std::string& address) {
        grpc::ServerBuilder builder;
        builder.AddListeningPort(address, grpc::InsecureServerCredentials());

        // 注册所有服务
        dynamic_cast<T*>(this)->RegisterServices(builder);

        cq_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();

        // 启动处理器线程
        dynamic_cast<T*>(this)->SpawnHandlers(cq_.get());

        RunEventLoop();
    }

    virtual ~RpcServer() {
        server_->Shutdown();
        cq_->Shutdown();
    }

protected:
    void RunEventLoop() {
        void* tag;
        bool ok;
        while (cq_->Next(&tag, &ok)) {
            if (ok) {
                GPR_ASSERT(ok);
                static_cast<CallDataBase*>(tag)->Proceed();
            }
        }
    }

    std::unique_ptr<grpc::ServerCompletionQueue> cq_;
    std::unique_ptr<grpc::Server> server_;
};





    
#endif // SERVER_IMPL_H
