#pragma onece

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

// ---------------- ServiceMethodTraits Specializations ----------------
template<>
struct ServiceMethodTraits<CountRequest> {
    using ResponseType = CountResponse;
    static constexpr auto Method = &DatabaseService::AsyncService::RequestGetUserFilesCount;
};


// ---------------- CountCall Definition ----------------
class CountCall : public CallData<CountRequest, CountCall> {
public:
    using Request  = CountRequest;
    using Response = CountResponse;
    using Base     = CallData<Request, CountCall>;

    CountCall(DatabaseService::AsyncService* service,
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

    void OnRequest(const Request& req, Response& resp)  {
        // Prepare statement for count
        auto stmt = SakilaDatabase.GetPreparedStatement(GET_USER_FILE_COUNT);
        stmt->setString(0, req.user());

        auto work = SakilaDatabase
            .AsyncQuery(stmt)
            .WithChainingPreparedCallback(
                [this](QueryCallback& cb, PreparedQueryResult r) {
                    if (!r || r->GetRowCount() == 0) {
                        this->reply_.set_code(1);
                    } else {
                        this->reply_.set_code(0);
                        std::cout<<"GetCount"<<r->Fetch()[0].GetInt32()<<'\n';
                        this->reply_.set_count(r->Fetch()[0].GetInt32());
                    }
                    this->status_ = FINISH;
                    this->responder_.Finish(this->reply_, grpc::Status::OK, this);
                }
            );
        processor_.AddCallback(std::move(work));
    }

private:
    friend class MySqlRpcServer;

    inline static AsyncCallbackProcessor<QueryCallback> processor_;
    inline static std::once_flag processor_init_flag_;
    inline static std::thread          processor_thread_;
    inline static std::atomic<bool>    processor_stop_flag_{false};
};

