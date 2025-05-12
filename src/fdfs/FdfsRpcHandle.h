#ifndef FDFS_RPC_HANDLERS_H
#define FDFS_RPC_HANDLERS_H

#include "RpcCalldata.h"
#include "FdfsConnPool.h"
#include "ThrdPool.h"
#include <string>
#include <exception>
//----------------------------------------------------------
// Upload 处理器
//----------------------------------------------------------
class UploadCall : public CallData<rpc::FdfsUploadRequest, UploadCall, rpc::FdfsService::AsyncService>
{
public:
    using Request = rpc::FdfsUploadRequest;
    using Response = rpc::FdfsUploadResponse;
    using Service = rpc::FdfsService::AsyncService;
    using Base = CallData<Request, UploadCall, Service>;

    UploadCall(rpc::FdfsService::AsyncService *service,
               grpc::ServerCompletionQueue *cq)
        : Base(service, cq) {}

    void OnRequest(const Request &req, Response &resp)
    {
        try
        {
            WorkPool::Instance().Submit(
                [this, &req, &resp]()
                {
                    std::string path = req.path();
                    auto res = FdfsConnectionPool::Instance().upload(path);
                    resp.set_fileid(res.first);
                    resp.set_url(res.second);
                    this->rpc_finish();
                });
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Error]  Fdfs_Upload" << e.what() << std::endl;
        }
    }
};

//----------------------------------------------------------
// Delete 处理器
//----------------------------------------------------------

inline std::string _trim(const std::string &s) {
    auto b = s.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) return "";                      
    auto e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
}

class DeleteCall : public CallData<rpc::FdfsDeleteRequest, DeleteCall, rpc::FdfsService::AsyncService>
{
public:
    using Request = rpc::FdfsDeleteRequest;
    using Response = rpc::FdfsDeleteResponse;
    using Service = rpc::FdfsService::AsyncService ;
    using Base = CallData<Request, DeleteCall, Service>;

    DeleteCall(rpc::FdfsService::AsyncService *service,
               grpc::ServerCompletionQueue *cq)
        : Base(service, cq) {}

    void OnRequest(const Request &req, Response &resp)
    {
        try
        {
        WorkPool::Instance().Submit(
            [this,&req,&resp](){
                std::string fileid = req.fileid();
                resp.set_success(FdfsConnectionPool::Instance().remove(_trim(fileid)));
                this->rpc_finish();
        });
        }catch(const std::exception &e)
        {
                std::cerr << "[Error]  Fdfs_Upload" << e.what() << std::endl;
        }
    }
};



#endif // FDFS_RPC_HANDLERS_H