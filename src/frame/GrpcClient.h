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
#include "fdfs_rpc.grpc.pb.h"
#include "ThrdPool.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using rpc::DatabaseService;
using rpc::RegisterRequest;
using rpc::RegisterResponse;

using rpc::FdfsService;

template <
    typename ClientT,
    typename Req,
    typename Resp,
    auto AsyncMethod>
struct RpcAwaitable
{
    ClientT *client;
    Req request;
    Resp response;
    std::coroutine_handle<> handle;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h)
    {
        handle = h;
        // 调用 client 上的 ClientCall<AsyncMethod>
        client->template ClientCall<AsyncMethod>(std::move(request), this);
    }

    Resp await_resume() noexcept
    {
        return std::move(response);
    }
};

template <
    typename Service, // e.g. rpc::DatabaseService 或 rpc::FdfsService
    typename Req,
    typename Resp>
class GrpcClient
{
public:
    using StubType = typename Service::Stub;

    explicit GrpcClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(Service::NewStub(channel)),
          completion_thread_(&GrpcClient::AsyncCompleteRpc, this)
    {
    }

    ~GrpcClient()
    {
        cq_.Shutdown();
        if (completion_thread_.joinable())
            completion_thread_.join();
    }

    // awaitable 需要访问私有成员
    template <typename Cli, typename Rq, typename Rp, auto AM>
    friend struct RpcAwaitable;

private:
    // 基础 Tag，用来在 CompletionQueue 回调
    struct TagBase
    {
        virtual ~TagBase() = default;
        virtual void OnComplete(bool ok) = 0;
    };

    // 每一次异步调用都会 new 一个 CallData
    template <auto AsyncMethod>
    struct CallData : TagBase
    {
        RpcAwaitable<GrpcClient, Req, Resp, AsyncMethod> *aw;
        Resp reply;
        grpc::ClientContext ctx;
        grpc::Status status;
        std::unique_ptr<grpc::ClientAsyncResponseReader<Resp>> reader;

        CallData(decltype(aw) a) : aw(a) {}

        void OnComplete(bool ok) override
        {
            if (ok && status.ok())
            {
                aw->response = std::move(reply);
                WorkPool::Instance().Submit([this]()
                                            { aw->handle.resume(); delete this; });
            }
            else
            {
                std::cerr << "RPC failed: "
                          << status.error_message() << std::endl;
                delete this;
            }
        }
    };

    // 真正发起 RPC 的入口，被 RpcAwaitable 调用
    template <auto AsyncMethod>
    void ClientCall(Req req,
                    RpcAwaitable<GrpcClient, Req, Resp, AsyncMethod> *aw)
    {
        auto *cd = new CallData<AsyncMethod>{aw};
        cd->reader = (stub_.get()->*AsyncMethod)(&cd->ctx,
                                                 std::move(req),
                                                 &cq_);
        cd->reader->StartCall();
        cd->reader->Finish(&cd->reply, &cd->status, cd);
    }

    // gRPC 的 CompletionQueue 循环
    void AsyncCompleteRpc()
    {
        void *tag;
        bool ok;
        while (cq_.Next(&tag, &ok))
        {
            static_cast<TagBase *>(tag)->OnComplete(ok);
        }
    }

    std::unique_ptr<StubType> stub_;
    grpc::CompletionQueue cq_;
    std::thread completion_thread_;
};

// mysql调用
template <typename Req, typename Resp>
inline auto MysqlRegisterCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncregisterUser>{client, std::move(req)};
}
template <typename Req, typename Resp>
inline auto MysqlLoginCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncloginUser>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlInstantUploadCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncInstantUpload>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlUploadFileCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncUploadFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlGetUserFilesCountCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncGetUserFilesCount>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlGetUserFileListCall(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncGetUserFileList>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlShareFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncShareFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlDeleteFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncDeleteFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlPvFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncPvFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlPvShareFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncPvShareFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlCancelShareFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncCancelShareFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlSaveFile(GrpcClient<rpc::DatabaseService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>, Req, Resp, &DatabaseService::Stub::PrepareAsyncSaveFile>{client, std::move(req)};
}

template <typename Req, typename Resp>
inline auto MysqlGetShareFilesCountCall( GrpcClient<rpc::DatabaseService, Req, Resp>* client,Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>,Req, Resp,&rpc::DatabaseService::Stub::PrepareAsyncGetShareFilesCount>(client, std::move(req));
}
template <typename Req, typename Resp>
inline auto MysqlGetShareFileListCall( GrpcClient<rpc::DatabaseService, Req, Resp>* client,Req req)
{
   return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>,Req, Resp,&rpc::DatabaseService::Stub::PrepareAsyncGetShareFileList>(client, std::move(req));
}
        
template <typename Req, typename Resp>
inline auto MysqlGetRankingFileListCall(
GrpcClient<rpc::DatabaseService, Req, Resp>* client,Req req)
{
    return RpcAwaitable<GrpcClient<rpc::DatabaseService, Req, Resp>,Req, Resp,&rpc::DatabaseService::Stub::PrepareAsyncGetRankingFileList>(client, std::move(req));
}
// 在模板调用区段后追加：






// fdfs调用

template <typename Req, typename Resp>
inline auto FdfslUploadFileCall(GrpcClient<rpc::FdfsService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::FdfsService, Req, Resp>, Req, Resp, &rpc::FdfsService::Stub::PrepareAsyncUpload>{client, std::move(req)};
}


template <typename Req, typename Resp>
inline auto FdfslDeleteFileCall(GrpcClient<rpc::FdfsService, Req, Resp> *client, Req req)
{
    return RpcAwaitable<GrpcClient<rpc::FdfsService, Req, Resp>, Req, Resp, &rpc::FdfsService::Stub::PrepareAsyncDelete>{client, std::move(req)};
}


#endif