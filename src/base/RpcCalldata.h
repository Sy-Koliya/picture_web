
#ifndef RPCCALLDATA_H
#define RPCCALLDATA_H
#include <memory>
#include <string>
#include <iostream>

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"

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
    //void  OnRequest(){Req req, Resp rsp}

public:
    DatabaseService::AsyncService*        service_;
    ServerCompletionQueue*                cq_;
    ServerContext                         ctx_;
    Req                                   request_;
    Resp                                  reply_;
    ServerAsyncResponseWriter<Resp>       responder_;
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus                            status_;
};

#endif