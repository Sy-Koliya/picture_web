#ifndef API_MD5_DB_H
#define API_MD5_DB_H

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
//秒传功能

using rpc::Md5Request;
using rpc::Md5Response;

template<>
struct ServiceMethodTraits<Md5Request> {
    using ResponseType = Md5Response;
    static constexpr auto Method =
      &rpc::DatabaseService::AsyncService::RequestInstantUpload;
};

class InstantUploadCall
  : public CallData<Md5Request, InstantUploadCall>
{
public:
    InstantUploadCall(
      rpc::DatabaseService::AsyncService* service,
      grpc::ServerCompletionQueue* cq)
      : CallData<Md5Request, InstantUploadCall>(service, cq)
    {
        std::call_once(init_flag_, [](){
            processor_thread_ = std::thread([]{
                while (!stop_flag_.load()) {
                    processor_.ProcessReadyCallbacks();
                    std::this_thread::sleep_for(
                      std::chrono::milliseconds(50));
                }
            });
        });
    }

    void OnRequest(const Md5Request& req, Md5Response& resp) override {
        // 1. 查 file_info.count
        auto stmt1 = SakilaDatabase.GetPreparedStatement(
                       CHECK_FILE_REF_COUNT);
        stmt1->setString(0, req.md5());
        auto qc = SakilaDatabase
          .AsyncQuery(stmt1)
          .WithChainingPreparedCallback(
            [this, &req, &resp](QueryCallback& cb1,
                                PreparedQueryResult r1) {
              if (!r1 || r1->GetRowCount() == 0) {
                resp.set_code(1);  // Md5Failed
                finish(resp);
                return;
              }
              int file_ref = r1->Fetch()[0].GetInt32();

              // 2. 查用户是否已拥有此文件
              auto stmt2 = SakilaDatabase.GetPreparedStatement(
                             CHECK_USER_FILE);
              stmt2->setString(0, req.user());
              stmt2->setString(1, req.md5());
              stmt2->setString(2, req.filename());
              cb1.SetNextQuery(
                SakilaDatabase.AsyncQuery(stmt2)
              ).WithChainingPreparedCallback(
                [this, file_ref, &req, &resp](QueryCallback& cb2,
                                              PreparedQueryResult r2) {
                  if (r2 && r2->GetRowCount()>0) {
                    resp.set_code(5);  // Md5FileExit
                    finish(resp);
                  } else {
                    // 3. 更新 file_info.count
                    auto stmt3 = SakilaDatabase.GetPreparedStatement(
                                   UPDATE_FILE_INFO_COUNT);
                    stmt3->setInt32(0, file_ref+1);
                    stmt3->setString(1, req.md5());
                    cb2.SetNextQuery(
                      SakilaDatabase.AsyncQuery(stmt3)
                    ).WithChainingPreparedCallback(
                      [this, &req, &resp](QueryCallback& cb3,
                                          PreparedQueryResult) {
                        // 4. 插入 user_file_list
                        auto stmt4 = SakilaDatabase.GetPreparedStatement(
                                       INSERT_USER_FILE);
                        stmt4->setString(0, req.user());
                        stmt4->setString(1, req.md5());
                        stmt4->setString(2, now_str());
                        stmt4->setString(3, req.filename());
                        stmt4->setInt32(4, 0);
                        stmt4->setInt32(5, 0);
                        cb3.SetNextQuery(
                          SakilaDatabase.AsyncQuery(stmt4)
                        ).WithChainingPreparedCallback(
                          [this, &req, &resp](QueryCallback& cb4,
                                              PreparedQueryResult) {
                            // 5. 更新或插入 user_file_count
                            auto stmt5 = SakilaDatabase.GetPreparedStatement(
                                           GET_USER_FILE_COUNT);
                            stmt5->setString(0, req.user());
                            cb4.SetNextQuery(
                              SakilaDatabase.AsyncQuery(stmt5)
                            ).WithChainingPreparedCallback(
                              [this, &req, &resp](QueryCallback& cb5,
                                                  PreparedQueryResult r5) {
                                if (!r5 || r5->GetRowCount()==0) {
                                  auto ins = SakilaDatabase.GetPreparedStatement(
                                               INSERT_USER_FILE_COUNT);
                                  ins->setString(0, req.user());
                                  ins->setInt32(1,1);
                                  processor_.AddCallback(
                                    SakilaDatabase.AsyncQuery(ins)
                                      .WithSimpleCallback(
                                        [this,&resp](auto){
                                          resp.set_code(0);
                                          finish(resp);
                                        }));
                                } else {
                                  int cnt = r5->Fetch()[0].GetInt32();
                                  auto upd = SakilaDatabase.GetPreparedStatement(
                                               UPDATE_USER_FILE_COUNT);
                                  upd->setInt32(0,cnt+1);
                                  upd->setString(1,req.user());
                                  processor_.AddCallback(
                                    SakilaDatabase.AsyncQuery(upd)
                                      .WithSimpleCallback(
                                        [this,&resp](auto){
                                          resp.set_code(0);
                                          finish(resp);
                                        }));
                                }
                              });
                          });
                      });
                  }
                });
            });

        processor_.AddCallback(std::move(qc));
    }

private:
    void finish(Md5Response& r) {
        status_ = FINISH;
        responder_.Finish(r, grpc::Status::OK, this);
    }

    static std::string now_str() {
        char buf[64];
        auto t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%F %T",
                      std::localtime(&t));
        return buf;
    }

    inline static AsyncCallbackProcessor<QueryCallback> processor_;
    inline static std::once_flag               init_flag_;
    inline static std::thread                   processor_thread_;
    inline static std::atomic<bool>             stop_flag_{false};
};

#endif // API_MD5_DB_H
