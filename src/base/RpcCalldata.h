#ifndef RPCCALLDATA_H
#define RPCCALLDATA_H

#include <memory>
#include <string>
#include <iostream>
#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "fdfs_rpc.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;
using grpc::Status;


struct CallDataBase {
    virtual void Proceed() = 0;
    virtual ~CallDataBase() = default;
};

template<typename T>
struct ServiceMethodTraits;


template<>
struct ServiceMethodTraits<rpc::FdfsUploadRequest> {
    using ResponseType = rpc::FdfsUploadResponse;
    static constexpr auto Method = 
        &rpc::FdfsService::AsyncService::RequestUpload;
};

template<>
struct ServiceMethodTraits<rpc::FdfsDeleteRequest> {
    using ResponseType = rpc::FdfsDeleteResponse;
    static constexpr auto Method = 
        &rpc::FdfsService::AsyncService::RequestDelete;
};


template<
    typename Req,
    typename Derived,
    typename ServiceType = rpc::DatabaseService::AsyncService
>
class CallData : public CallDataBase {
public:
    using Resp = typename ServiceMethodTraits<Req>::ResponseType;
    static constexpr auto RequestMethod = ServiceMethodTraits<Req>::Method;
    CallData(ServiceType* service, ServerCompletionQueue* cq)
    : service_(service)
    , cq_(cq)
    , responder_(&ctx_)
    , status_(CREATE)
  {
      Proceed();
  }

  void Proceed() override {
      if (status_ == CREATE) {
          status_ = PROCESS;
          // 注册事件到 CompletionQueue
          (service_->*RequestMethod)(
              &ctx_,
              &request_,
              &responder_,
              cq_, cq_,
              this  // tag
          );
      } else if (status_ == PROCESS) {
          // 收到客户端请求，先创建下一个 Handler
          new Derived(service_, cq_);
          // 执行用户业务逻辑
          //OnRequest 中需要使用Finish
          try{
          static_cast<Derived*>(this)->OnRequest(request_, reply_);
          } catch (const std::exception& e) {
              std::cerr << "Exception in OnRequest: " << e.what() << std::endl;
          }
          // 标记状态为 FINISH，等待回复完成
          status_ = FINISH;
      } else {
          // FINISH：清理资源
          delete this;
      }
  }

protected:
  void rpc_finish(){

    status_ = FINISH;
    responder_.Finish(reply_, grpc::Status::OK, this);
  }

  ServiceType*                  service_;  
  ServerCompletionQueue*        cq_;
  ServerContext                 ctx_;
  Req                          request_;
  Resp                         reply_;
  ServerAsyncResponseWriter<Resp> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus                    status_;
};



#endif 