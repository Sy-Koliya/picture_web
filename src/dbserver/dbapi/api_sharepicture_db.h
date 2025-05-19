#pragma once

#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "RpcCalldata.h"
#include "Implementation/SakilaDatabase.h"
#include <string>
#include <random>
#include "tools.h"

using rpc::DatabaseService;

// 生成随机字符串，用于一次性 URL MD5
static std::string RandomString_(std::size_t len)
{
    static constexpr char charset[] =
        "abcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist{
        0, sizeof(charset) - 2 // 最后一位为 '\0'
    };
    std::string s;
    s.reserve(len);
    for (std::size_t i = 0; i < len; ++i)
    {
        s += charset[dist(rng)];
    }
    return s;
}

// —— 1. SharePicture ——
template <>
struct ServiceMethodTraits<rpc::SharePictureRequest> {
  using ResponseType = rpc::SharePictureResponse;
  static constexpr auto Method =
    &DatabaseService::AsyncService::RequestSharePicture;
};

class SharePictureCall
  : public CallData<rpc::SharePictureRequest, SharePictureCall>
{
public:
  SharePictureCall(DatabaseService::AsyncService* svc,
                   grpc::ServerCompletionQueue* cq)
    : CallData<rpc::SharePictureRequest, SharePictureCall>(svc, cq) {}

  void OnRequest(const rpc::SharePictureRequest& req,
                 rpc::SharePictureResponse& reply)
  {
    int code = SyncSharePicture(req.user(), req.md5(), req.filename());
    reply.set_code(code);
    if (code == 0) {
      reply.set_urlmd5(generated_urlmd5_);
    }
    rpc_finish();
  }

private:
  // 生成一次性的 urlmd5 并插入 share_picture_list + 更新计数表
  int SyncSharePicture(const std::string& user,
                       const std::string& md5,
                       const std::string& filename)
  {
    // 1) 随机生成 urlmd5
    generated_urlmd5_ = RandomString_(32);

    // 2) 当前时间
    std::string ts = _now_str();
    auto& db = SakilaDatabase;
    try {
      // 插入分享列表 (包含 `key` 列)
      std::string ins =
        "INSERT INTO share_picture_list "
        "(`user`, filemd5, file_name, urlmd5, `key`, pv, create_time) "
        "VALUES ('" + user + "','" + md5 + "','" + filename + "','" +
        generated_urlmd5_ + "','','0','" + ts + "')";
      db.Execute(ins.c_str());

      // 更新 user_file_count
      std::string cntKey = user + "_share_picture_list_count";
      std::string selCnt =
        "SELECT `count` FROM user_file_count "
        "WHERE `user` = '" + cntKey + "'";
      auto r = db.Query(selCnt.c_str());
      if (!r || r->GetRowCount() == 0) {
        std::string insCnt =
          "INSERT INTO user_file_count (`user`, `count`) "
          "VALUES('" + cntKey + "', 1)";
        db.Execute(insCnt.c_str());
      } else {
        int c = r->Fetch()[0].GetInt32() + 1;
        std::string updCnt =
          "UPDATE user_file_count "
          "SET `count` = " + std::to_string(c) +
          " WHERE `user` = '" + cntKey + "'";
        db.Execute(updCnt.c_str());
      }
      return 0;
    } catch (...) {
      return 1;
    }
  }

  std::string generated_urlmd5_;
};

// —— 2. GetSharePicturesCount ——
template <>
struct ServiceMethodTraits<rpc::GetSharePicturesCountRequest> {
  using ResponseType = rpc::GetSharePicturesCountResponse;
  static constexpr auto Method =
    &DatabaseService::AsyncService::RequestGetSharePicturesCount;
};

class GetSharePicturesCountCall
  : public CallData<rpc::GetSharePicturesCountRequest,
                    GetSharePicturesCountCall>
{
public:
  GetSharePicturesCountCall(DatabaseService::AsyncService* svc,
                            grpc::ServerCompletionQueue* cq)
    : CallData<rpc::GetSharePicturesCountRequest,
               GetSharePicturesCountCall>(svc, cq) {}

  void OnRequest(const rpc::GetSharePicturesCountRequest& req,
                 rpc::GetSharePicturesCountResponse& reply)
  {
    const auto& user = req.user();
    std::string cntKey = user + "_share_picture_list_count";
    try {
      auto& db = SakilaDatabase;
      std::string sel =
        "SELECT `count` FROM user_file_count "
        "WHERE `user` = '" + cntKey + "'";
      auto r = db.Query(sel.c_str());
      int total = (r && r->GetRowCount() > 0)
                    ? r->Fetch()[0].GetInt32()
                    : 0;
      reply.set_code(0);
      reply.set_total(total);
    } catch (...) {
      reply.set_code(1);
      reply.set_total(0);
    }
    rpc_finish();
  }
};

// —— 3. GetSharePicturesList ——
template <>
struct ServiceMethodTraits<rpc::GetSharePicturesListRequest> {
  using ResponseType = rpc::GetSharePicturesListResponse;
  static constexpr auto Method =
    &DatabaseService::AsyncService::RequestGetSharePicturesList;
};

class GetSharePicturesListCall
  : public CallData<rpc::GetSharePicturesListRequest,
                    GetSharePicturesListCall>
{
public:
  GetSharePicturesListCall(DatabaseService::AsyncService* svc,
                           grpc::ServerCompletionQueue* cq)
    : CallData<rpc::GetSharePicturesListRequest,
               GetSharePicturesListCall>(svc, cq) {}

  void OnRequest(const rpc::GetSharePicturesListRequest& req,
                 rpc::GetSharePicturesListResponse& reply)
  {
    const auto& user = req.user();
    int start = req.start();
    int cnt   = req.count();

    auto& db = SakilaDatabase;
    // a) 总数
    std::string cntKey = user + "_share_picture_list_count";
    try {
      auto r0 = db.Query(
        ("SELECT `count` FROM user_file_count "
         "WHERE `user` = '" + cntKey + "'").c_str());
      int total = (r0 && r0->GetRowCount() > 0)
                    ? r0->Fetch()[0].GetInt32()
                    : 0;
      reply.set_total(total);
    } catch (...) {
      reply.set_total(0);
    }

    // b) 列表
    try {
      std::string q =
        "SELECT sp.`user`, sp.filemd5, sp.file_name, sp.urlmd5,"
        " sp.pv, sp.create_time, fi.size "
        "FROM share_picture_list AS sp "
        "JOIN file_info AS fi ON fi.md5 = sp.filemd5 "
        "WHERE sp.`user` = '" + user + "' "
        "ORDER BY sp.create_time DESC "
        "LIMIT " + std::to_string(start) + ", " + std::to_string(cnt);
      auto rs = db.Query(q.c_str());
      if (rs) {
          auto totalRows = rs->GetRowCount();
        for (size_t i = 0; i < totalRows; ++i) {
          auto* f = reply.add_files();
          auto row = rs->Fetch();
          f->set_user(       row[0].GetString());
          f->set_filemd5(    row[1].GetString());
          f->set_file_name(  row[2].GetString());
          f->set_urlmd5(     row[3].GetString());
          f->set_pv(         row[4].GetInt32());
          f->set_create_time(row[5].GetString());
          f->set_size(       row[6].GetInt32());
          rs->NextRow();
        }
      }
      reply.set_code(0);
    } catch (...) {
      reply.set_code(1);
    }
    rpc_finish();
  }
};

// —— 4. CancelSharePicture ——
template <>
struct ServiceMethodTraits<rpc::CancelSharePictureRequest> {
  using ResponseType = rpc::CancelSharePictureResponse;
  static constexpr auto Method =
    &DatabaseService::AsyncService::RequestCancelSharePicture;
};

class CancelSharePictureCall
  : public CallData<rpc::CancelSharePictureRequest,
                    CancelSharePictureCall>
{
public:
  CancelSharePictureCall(DatabaseService::AsyncService* svc,
                         grpc::ServerCompletionQueue* cq)
    : CallData<rpc::CancelSharePictureRequest,
               CancelSharePictureCall>(svc, cq) {}

  void OnRequest(const rpc::CancelSharePictureRequest& req,
                 rpc::CancelSharePictureResponse& reply)
  {
    int code = SyncCancelShare(req.user(), req.urlmd5());
    reply.set_code(code);
    rpc_finish();
  }

private:
  int SyncCancelShare(const std::string& user,
                      const std::string& urlmd5)
  {
    auto& db = SakilaDatabase;
    try {
      // 1) 确认记录存在
      std::string sel =
        "SELECT 1 FROM share_picture_list "
        "WHERE `user` = '" + user + "' "
        "AND urlmd5 = '" + urlmd5 + "'";
      auto r = db.Query(sel.c_str());
      if (!r || r->GetRowCount() == 0) {
        return 0;  // nothing to do
      }
      // 2) 减 count
      std::string cntKey = user + "_share_picture_list_count";
      auto r0 = db.Query(
        ("SELECT `count` FROM user_file_count "
         "WHERE `user` = '" + cntKey + "'").c_str());
      int c = (r0 && r0->GetRowCount() > 0)
                ? r0->Fetch()[0].GetInt32()
                : 0;
      if (c > 0) {
        db.Execute(
          ("UPDATE user_file_count "
           "SET `count` = " + std::to_string(c-1) +
           " WHERE `user` = '" + cntKey + "'").c_str());
      }
      // 3) 删除 share_picture_list
      db.Execute(
        ("DELETE FROM share_picture_list "
         "WHERE `user` = '" + user + "' "
         "AND urlmd5 = '" + urlmd5 + "'").c_str());
      return 0;
    } catch (...) {
      return 1;
    }
  }
};

// —— 5. BrowsePicture ——
template <>
struct ServiceMethodTraits<rpc::BrowsePictureRequest> {
  using ResponseType = rpc::BrowsePictureResponse;
  static constexpr auto Method =
    &DatabaseService::AsyncService::RequestBrowsePicture;
};

class BrowsePictureCall
  : public CallData<rpc::BrowsePictureRequest,
                    BrowsePictureCall>
{
public:
  BrowsePictureCall(DatabaseService::AsyncService* svc,
                    grpc::ServerCompletionQueue* cq)
    : CallData<rpc::BrowsePictureRequest,
               BrowsePictureCall>(svc, cq) {}

  void OnRequest(const rpc::BrowsePictureRequest& req,
                 rpc::BrowsePictureResponse& reply)
  {
    try {
      auto& db = SakilaDatabase;
      // 1) 查询分享记录
      auto r0 = db.Query(
        ("SELECT `user`, filemd5, pv, create_time "
         "FROM share_picture_list "
         "WHERE urlmd5 = '" + req.urlmd5() + "'").c_str());
      if (!r0 || r0->GetRowCount() == 0) throw std::runtime_error("not found");
      auto row0 = r0->Fetch();
      std::string user        = row0[0].GetString();
      std::string filemd5     = row0[1].GetString();
      int         pv          = row0[2].GetInt32();
      std::string create_time = row0[3].GetString();

      // 2) 查真正的 URL
      auto r1 = db.Query(
        ("SELECT url FROM file_info WHERE md5 = '" + filemd5 + "'").c_str());
      if (!r1 || r1->GetRowCount() == 0) throw std::runtime_error("url not found");
      std::string url = r1->Fetch()[0].GetString();

      // 3) 更新 pv
      db.Execute(
        ("UPDATE share_picture_list "
         "SET pv = " + std::to_string(pv + 1) +
         " WHERE urlmd5 = '" + req.urlmd5() + "'").c_str());

      // 4) 填充回复
      reply.set_code(0);
      reply.set_pv(pv);
      reply.set_url(url);
      reply.set_user(user);
      reply.set_time(create_time);
    } catch (...) {
      reply.set_code(1);
    }
    rpc_finish();
  }
};
