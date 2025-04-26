#ifndef GRPCCLIENT_H
#define GROCCLIENT_H

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <coroutine>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include "mysql_rpc.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;

template <typename RpcRequest, typename RpcResponse>
class MysqlClient
{
public:
  explicit MysqlClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(DatabaseService::NewStub(channel)),
        completion_thread_(&MysqlClient::AsyncCompleteRpc, this) {}

  ~MysqlClient()
  {
    cq_.Shutdown();
    if (completion_thread_.joinable())
    {
      completion_thread_.join();
    }
  }

  // 协程化的异步调用接口

private:

  template<typename AsyncMethod>
  void ClientCall(RpcRequest req, RpcAwaitable<RpcRequest,RpcResponse,AsyncMethod>* aw) {
      struct AsyncClientCall {
          decltype(aw)* awaiter;
          RpcResponse      reply;
          ClientContext    ctx;
          Status           status;
          std::unique_ptr<ClientAsyncResponseReader<RpcResponse>> reader;
      };

      auto* call = new AsyncClientCall{aw};
      // 这里用 AsyncMethod 调用对应的 PrepareAsyncXxx
      call->reader = (stub_.get()->*AsyncMethod)(
          &call->ctx, std::move(req), &cq_);
      call->reader->StartCall();
      call->reader->Finish(&call->reply, &call->status, call);
  }


  void AsyncCompleteRpc()
  {
    void *got_tag;
    bool ok;
    while (cq_.Next(&got_tag, &ok))
    {
      auto *call = static_cast<decltype(std::declval<MysqlClient>().ClientCall)::element_type::AsyncClientCall *>(got_tag);

      if (ok && call->status.ok())
      {
        // 把结果拷贝回 awaitable
        call->awaiter->response = std::move(call->reply);
        // resume 协程
        call->awaiter->handle.resume();
      }
      else
      {
        std::cerr << "RPC failed: "
                  << call->status.error_message()
                  << std::endl;
        // 这里也可以设置一个错误标志在 awaiter 里
      }
      delete call;
    }
  }

  std::unique_ptr<DatabaseService::Stub> stub_;
  grpc::CompletionQueue cq_;
  std::thread completion_thread_;
};

// 1. 在 RpcAwaitable 里加一个 AsyncMethod 成员
template<typename Req, typename Resp,typename AsyncMethod >
struct RpcAwaitable
{
  MysqlClient<RpcRequest, RpcResponse> *client;
  RpcRequest request;
  RpcResponse response;           
  std::coroutine_handle<> handle; // 协程句柄

  bool await_ready() const noexcept { return false; }

  // suspend 时把自己传给 client，启动 RPC
  void await_suspend(std::coroutine_handle<> h)
  {
    handle = h;
    client->template ClientCall<AsyncMethod>(std::move(request), this);
  }

  // resume 后把 response 返给 co_await 表达式
  RpcResponse await_resume() noexcept
  {
    return std::move(response);
  }
};


#endif