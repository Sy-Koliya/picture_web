
#include "GrpcServer.h"
#include "mysql_rpc.grpc.pb.h"
#include "dbapi/commom.h"



class MySqlRpcServer : public RpcServer<MySqlRpcServer> {
    public:
        void RegisterServices(grpc::ServerBuilder& builder) {
            builder.RegisterService(&db_service_);
        }
    
        void SpawnHandlers(grpc::ServerCompletionQueue* cq) {
            // 启动数据库服务处理器
            new LoginUserCall(&db_service_, cq_.get());
            new RegisterUserCall(&db_service_, cq_.get());
            new InstantUploadCall(&db_service_, cq_.get());
            new UploadFileCall(&db_service_, cq_.get());
        }
        ~MySqlRpcServer(){
            RegisterUserCall::processor_stop_flag_.store(true);
            LoginUserCall::processor_stop_flag_.store(true);
            InstantUploadCall::processor_stop_flag_.store(true);
            UploadFileCall::processor_stop_flag_.store(true);
            if (RegisterUserCall::processor_thread_.joinable())
                RegisterUserCall::processor_thread_.join();
            if (LoginUserCall::processor_thread_.joinable())
                LoginUserCall::processor_thread_.join();
            if (InstantUploadCall::processor_thread_.joinable())
                InstantUploadCall::processor_thread_.join();
            if (UploadFileCall::processor_thread_.joinable())
                UploadFileCall::processor_thread_.join();
        }
    private:
    rpc::DatabaseService::AsyncService db_service_;

};