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
// 秒传功能

using rpc::Md5Request;
using rpc::Md5Response;

template <>
struct ServiceMethodTraits<Md5Request>
{
  using ResponseType = Md5Response;
  static constexpr auto Method =
      &rpc::DatabaseService::AsyncService::RequestInstantUpload;
};

class InstantUploadCall
    : public CallData<Md5Request, InstantUploadCall>
{
public:
  InstantUploadCall(
      rpc::DatabaseService::AsyncService *service,
      grpc::ServerCompletionQueue *cq)
      : CallData<Md5Request, InstantUploadCall>(service, cq)
  {
    std::call_once(processor_init_flag_, []()
                   {
        // jthread 的析构会自动 request_stop() 并 join()
        processor_thread_ = std::thread([]{
           while (!processor_stop_flag_.load()) {
             processor_.ProcessReadyCallbacks();
             std::this_thread::sleep_for(std::chrono::milliseconds(50));
           }
       }); });
  }

  void OnRequest(const Md5Request &req, Md5Response &resp)
  {
    struct UploadContext
    {
      std::string user;
      std::string md5;
      std::string filename;
      uint32_t file_ref_count = 0;
      uint32_t user_file_count = 0;
    };
    auto ctx = std::make_shared<UploadContext>();
    ctx->user = request_.user();
    ctx->md5 = request_.md5();
    ctx->filename = request_.filename();

    // 2. 准备第一条 SQL：查询 file_info.count
    auto stmt_check_ref =
        SakilaDatabase.GetPreparedStatement(CHECK_MD5_FILE_REF_COUNT);
    stmt_check_ref->setString(0, ctx->md5);

    // 3. 发起异步链式调用
    auto chain = SakilaDatabase
                     .AsyncQuery(stmt_check_ref)
                     // —— 第 1 步回调：处理 “SELECT count FROM file_info WHERE md5 = ?”
                     .WithChainingPreparedCallback(
                         [ctx, this](QueryCallback &callback,
                                     PreparedQueryResult query_result)
                         {
                           // 如果没有任何行，秒传直接失败
                           if (!query_result || query_result->GetRowCount() == 0)
                           {
                             this->reply_.set_code(1);
                             this->finish(this->reply_);
                             return;
                           }
                           // 读取 count
                           ctx->file_ref_count = query_result->Fetch()[0].GetUInt32();

                           // 继续第 2 步：检查 user_file_list
                           auto stmt_check_user =
                               SakilaDatabase.GetPreparedStatement(CHECK_USER_FILE_LIST_EXIST);
                           stmt_check_user->setString(0, ctx->user);
                           stmt_check_user->setString(1, ctx->md5);
                           stmt_check_user->setString(2, ctx->filename);

                           // 设置下一查询
                           callback.SetNextQuery(
                               SakilaDatabase.AsyncQuery(stmt_check_user));
                         })
                     // —— 第 2 步回调：处理 “SELECT md5 FROM user_file_list…”
                     .WithChainingPreparedCallback(
                         [ctx, this](QueryCallback &callback,
                                     PreparedQueryResult query_result)
                         {
                           if (query_result && query_result->GetRowCount() > 0)
                           {
                             // 用户已上传过
                             this->reply_.set_code(5);
                             this->finish(this->reply_);
                             return;
                           }
                           // 第 3 步：更新 file_info.count
                           auto stmt_update_ref =
                               SakilaDatabase.GetPreparedStatement(UPDATE_FILE_INFO_COUNT);
                           stmt_update_ref->setUInt32(0, ctx->file_ref_count + 1);
                           stmt_update_ref->setString(1, ctx->md5);

                           callback.SetNextQuery(
                               SakilaDatabase.AsyncQuery(stmt_update_ref));
                         })
                     // —— 第 3 步回调：插入 user_file_list
                     .WithChainingPreparedCallback(
                         [ctx, this](QueryCallback &callback,
                                     PreparedQueryResult /*ignored*/)
                         {
                           auto stmt_insert_user_file =
                               SakilaDatabase.GetPreparedStatement(INSERT_USER_FILE_LIST);
                           stmt_insert_user_file->setString(0, ctx->user);
                           stmt_insert_user_file->setString(1, ctx->md5);
                           stmt_insert_user_file->setString(2, now_str());
                           stmt_insert_user_file->setString(3, ctx->filename);
                           stmt_insert_user_file->setUInt32(4, 0);
                           stmt_insert_user_file->setUInt32(5, 0);

                           callback.SetNextQuery(
                               SakilaDatabase.AsyncQuery(stmt_insert_user_file));
                         })
                     // —— 第 4 步回调：查询 user_file_count
                     .WithChainingPreparedCallback(
                         [ctx, this](QueryCallback &callback,
                                     PreparedQueryResult /*ignored*/)
                         {
                           auto stmt_sel_count =
                               SakilaDatabase.GetPreparedStatement(SELECT_USER_FILE_COUNT);
                           stmt_sel_count->setString(0, ctx->user);
                           callback.SetNextQuery(
                               SakilaDatabase.AsyncQuery(stmt_sel_count));
                         })
                     // —— 第 5 步回调：插入或更新 user_file_count
                     .WithChainingPreparedCallback(
                         [ctx, this](QueryCallback & /*callback*/,
                                     PreparedQueryResult query_result)
                         {
                           if (!query_result || query_result->GetRowCount() == 0)
                           {
                             // 首次插入
                             ctx->user_file_count = 1;
                             auto stmt_ins_count =
                                 SakilaDatabase.GetPreparedStatement(INSERT_USER_FILE_COUNT);
                             stmt_ins_count->setString(0, ctx->user);
                             stmt_ins_count->setUInt32(1, ctx->user_file_count);
                             // 直接执行，不再链式调用
                             SakilaDatabase.AsyncQuery(stmt_ins_count)
                                 .WithChainingPreparedCallback(
                                     [this](QueryCallback &, PreparedQueryResult)
                                     {
                                       this->reply_.set_code(0);
                                       this->finish(this->reply_);
                                     });
                           }
                           else
                           {
                             // 更新已有记录
                             ctx->user_file_count =
                                 query_result->Fetch()[0].GetUInt32() + 1;
                             auto stmt_upd_count =
                                 SakilaDatabase.GetPreparedStatement(UPDATE_USER_FILE_COUNT);
                             stmt_upd_count->setUInt32(0, ctx->user_file_count);
                             stmt_upd_count->setString(1, ctx->user);
                             SakilaDatabase.AsyncQuery(stmt_upd_count)
                                 .WithChainingPreparedCallback(
                                     [this](QueryCallback &, PreparedQueryResult)
                                     {
                                       this->reply_.set_code(0);
                                       this->finish(this->reply_);
                                     });
                           }
                         });

    processor_.AddCallback(std::move(chain));
  }

private:
  void finish(Md5Response &r)
  {
    status_ = FINISH;
    responder_.Finish(r, grpc::Status::OK, this);
  }

  static std::string now_str()
  {
    char buf[64];
    auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%F %T",
                  std::localtime(&t));
    return buf;
  }
  friend class MySqlRpcServer;
  inline static AsyncCallbackProcessor<QueryCallback> processor_;
  inline static std::once_flag processor_init_flag_;
  inline static std::thread processor_thread_;
  inline static std::atomic<bool> processor_stop_flag_{false};
};

#endif // API_MD5_DB_H
