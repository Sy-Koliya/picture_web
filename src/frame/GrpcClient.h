#ifndef GRPCCLIENT_H
#define GRPCCLIENT_H

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


template<typename Req, typename Resp,auto AsyncMethod >
struct RpcAwaitable;

template<typename Req, typename Resp>
class MysqlClient {
public:
    explicit MysqlClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(DatabaseService::NewStub(channel)),
        completion_thread_(&MysqlClient::AsyncCompleteRpc, this)
    {}

    ~MysqlClient() {
        cq_.Shutdown();
        if (completion_thread_.joinable())
            completion_thread_.join();
    }

    // co_await 调用
    template<auto AsyncMethod>
    RpcAwaitable<Req,Resp,AsyncMethod>
    make_awaitable(Req req) {
        return { this, std::move(req) };
    }

private:
   template<typename Rq, typename Rsp, auto AM>
     friend struct RpcAwaitable;

    struct TagBase {
        virtual ~TagBase() = default;
        virtual void OnComplete(bool ok) = 0;
    };

    // 嵌套模板：真正的 call context
    template<auto AsyncMethod>
    struct AsyncClientCall : TagBase {
        RpcAwaitable<Req,Resp,AsyncMethod>* awaiter;
        Resp      reply;
        ClientContext    ctx;
        Status           status;
        std::unique_ptr<ClientAsyncResponseReader<Resp>> reader;

        AsyncClientCall(decltype(awaiter) aw) : awaiter(aw) {}

        // Finish 回调后走这里
        void OnComplete(bool ok) override {
            if (ok && status.ok()) {
                awaiter->response = std::move(reply);
                awaiter->handle.resume();
            } else {
                std::cerr << "RPC failed: " 
                          << status.error_message() 
                          << std::endl;
            }
            delete this;
        }
    };

    // 启动一次 RPC，并把 AsyncClientCall<AsyncMethod> 作为 tag
    template<auto AsyncMethod>
    void ClientCall(Req req,
                    RpcAwaitable<Req,Resp,AsyncMethod>* aw)
    {
        auto* call = new AsyncClientCall<AsyncMethod>{ aw };
        call->reader = (stub_.get()->*AsyncMethod)(
            &call->ctx, std::move(req), &cq_);
        call->reader->StartCall();
        call->reader->Finish(&call->reply, &call->status, call);
    }

    // 唯一的 CompletionQueue 轮询点
    void AsyncCompleteRpc() {
        void* got_tag;
        bool ok;
        while (cq_.Next(&got_tag, &ok)) {
            static_cast<TagBase*>(got_tag)->OnComplete(ok);
        }
    }

    std::unique_ptr<DatabaseService::Stub> stub_;
    grpc::CompletionQueue               cq_;
    std::thread                         completion_thread_;
};

template<typename Req, typename Resp,auto AsyncMethod >
struct RpcAwaitable
{
  MysqlClient<Req, Resp> *client;
  Req request;
  Resp response;           
  std::coroutine_handle<> handle; // 协程句柄

  bool await_ready() const noexcept { return false; }

  // suspend 时把自己传给 client，启动 RPC
  void await_suspend(std::coroutine_handle<> h)
  {
    handle = h;
    client->template ClientCall<AsyncMethod>(std::move(request), this);
  }

  // resume 后把 response 返给 co_await 表达式
  Resp await_resume() noexcept
  {
    return std::move(response);
  }
};


#endif