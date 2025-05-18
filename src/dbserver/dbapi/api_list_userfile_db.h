
    #include <grpcpp/grpcpp.h>
    #include "mysql_rpc.grpc.pb.h"
    #include "AsyncCallbackProcessor.h"
    #include "RpcCalldata.h"
    #include "Implementation/SakilaDatabase.h"
    #include "QueryHolder.h"
    #include <mutex>
    #include <thread>
    #include <atomic>
    #include <chrono>

    using rpc::DatabaseService;
    using rpc::CountRequest;
    using rpc::CountResponse;
    using rpc::FilesListRequest;
    using rpc::FilesListResponse;


    template<>
    struct ServiceMethodTraits<FilesListRequest> {
        using ResponseType = FilesListResponse;
        static constexpr auto Method = &DatabaseService::AsyncService::RequestGetUserFileList;
    };


    class ListCall : public CallData<FilesListRequest, ListCall> {
    public:
        using Request  = FilesListRequest;
        using Response = FilesListResponse;
        using Base     = CallData<Request, ListCall>;

        ListCall(DatabaseService::AsyncService* service,
                grpc::ServerCompletionQueue* cq)
        : Base(service, cq)
        {
            std::call_once(processor_init_flag_, [](){
                processor_thread_ = std::thread([]{
                    while (!processor_stop_flag_.load()) {
                        processor_.ProcessReadyCallbacks();
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                });
            });
        }

        void OnRequest(const Request& req, Response& /*unused*/)  {
        // 1) 选择对应的 PreparedStatement key
    SakilaDatabaseStatements stmtKey;
        switch (req.order_by()) {
            case rpc::PV_ASC:
                stmtKey = GET_USER_FILES_LIST_ASC;
                break;
            case rpc::PV_DESC:
                stmtKey = GET_USER_FILES_LIST_DESC;
                break;
            default:
                stmtKey = GET_USER_FILES_LIST_NORMAL;
                break;
        }

        // 2) 准备 COUNT 语句
        auto countStmt = SakilaDatabase.GetPreparedStatement(GET_USER_FILE_COUNT);
        countStmt->setString(0, req.user_id());

        // 3) 准备 LIST 语句
        auto listStmt = SakilaDatabase.GetPreparedStatement(stmtKey);
        listStmt->setString(0, req.user_id());
        listStmt->setInt32(1, req.start());
        listStmt->setInt32(2, req.limit());

        // 4) 异步链式执行：先查总数，再查列表
        auto work = SakilaDatabase
            .AsyncQuery(countStmt)
            .WithChainingPreparedCallback(
                [this, listStmt](QueryCallback& cb, PreparedQueryResult result) {
                    int total = 0;
                    if (result && result->GetRowCount() > 0) {
                        total = result->Fetch()[0].GetInt32();
                    }
                    // 保存 total 到 reply_
                    this->reply_.set_total(total);

                    // 下一步：执行列表查询
                    cb.SetNextQuery(
                        SakilaDatabase.AsyncQuery(listStmt)
                    );
                }
            )
        .WithChainingPreparedCallback(
                [this](QueryCallback& cb, PreparedQueryResult result) {
                    if (result) {
                    
                    uint64_t totalRows = result->GetRowCount();
                        for (uint64_t i = 0; i < totalRows; ++i) {
                            auto row = result->Fetch();  // 先取当前行
                            auto* f = this->reply_.add_files();
                            f->set_user_id    (row[0].GetString());
                            f->set_file_md5   (row[1].GetString());
                            f->set_created_at (row[2].GetString());
                            f->set_filename   (row[3].GetString());
                            f->set_is_shared  (row[4].GetInt32());
                            f->set_view_count (row[5].GetInt32());
                            f->set_file_url   (row[6].GetString());
                            f->set_file_size  (row[7].GetInt64());
                            f->set_file_type  (row[8].GetString());
                            result->NextRow();
                        }
                    this->reply_.set_count(totalRows);
                    }else{
                        this->reply_.set_count(0);
                    }

                    // 完成 RPC
                    this->status_ = FINISH;
                    this->responder_.Finish(this->reply_, grpc::Status::OK, this);
                }
            );

        // 5) 提交给后台线程处理
        processor_.AddCallback(std::move(work));
    }


    private:
        friend class MySqlRpcServer;
        inline static AsyncCallbackProcessor<QueryCallback> processor_;
        inline static std::once_flag processor_init_flag_;
        inline static std::thread          processor_thread_;
        inline static std::atomic<bool>    processor_stop_flag_{false};
    };
