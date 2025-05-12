#include "GrpcServer.h"
#include "fdfs_rpc.grpc.pb.h"
#include "FdfsRpcHandle.h"

class FdfsServer:public RpcServer<FdfsServer>{
    public:
    void RegisterServices(grpc::ServerBuilder& builder) {
        builder.RegisterService(&fdfs_service_);
    }

    void SpawnHandlers(grpc::ServerCompletionQueue* cq) {
        new UploadCall(&fdfs_service_, cq);
        new DeleteCall(&fdfs_service_, cq);
    }
    rpc::FdfsService::AsyncService fdfs_service_;
};