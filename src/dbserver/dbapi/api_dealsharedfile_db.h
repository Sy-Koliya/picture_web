#pragma once
#include <grpcpp/grpcpp.h>
#include "mysql_rpc.grpc.pb.h"
#include "RedisClient.h"
#include "Implementation/SakilaDatabase.h"
#include <chrono>
#include "tools.h"


using rpc::DatabaseService;
using rpc::CancelShareFileRequest;
using rpc::CancelShareFileResponse;
using rpc::SaveFileRequest;
using rpc::SaveFileResponse;
using rpc::PvShareFileRequest;
using rpc::PvShareFileResponse;

static constexpr int debug=1;

// 1) CancelShareFile
template<>
struct ServiceMethodTraits<CancelShareFileRequest> {
    using ResponseType = CancelShareFileResponse;
    static constexpr auto Method =
        &DatabaseService::AsyncService::RequestCancelShareFile;
};
class CancelShareFileCall
    : public CallData<CancelShareFileRequest, CancelShareFileCall> {
public:
    CancelShareFileCall(DatabaseService::AsyncService* svc,
                        grpc::ServerCompletionQueue* cq)
      : CallData<CancelShareFileRequest, CancelShareFileCall>(svc, cq) {}
    void OnRequest(const CancelShareFileRequest& req,
                   CancelShareFileResponse& reply) {
        if (debug==1)
        std::cout<<"[DEBUG] CancelShareFileCall::OnRequest()"<<std::endl;
        reply.set_code(SyncCancel(req.user(), req.md5(), req.filename()));
        rpc_finish();
    }
private:
    int SyncCancel(const std::string& user,
                   const std::string& md5,
                   const std::string& filename) {
        try {
            auto& db    = SakilaDatabase;
            auto& redis = get_redis();
            std::string fileid = md5 + filename;
            // 1. Mysql: shared_status=0
            db.Execute(("UPDATE user_file_list SET shared_status=0 "
                        "WHERE user='"+user+"' AND md5='"+md5+
                        "' AND file_name='"+filename+"'").c_str());
            // 2. Mysql: 更新 share count
            auto r1 = db.Query(
                ("SELECT count FROM user_file_count "
                 "WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'"));
            if (!r1 || r1->GetRowCount()==0) return 1;
            int cnt = r1->Fetch()[0].GetInt32();
            if (cnt>0) {
                db.Execute(("UPDATE user_file_count SET count="+std::to_string(cnt-1)+
                            " WHERE user='xxx_share_xxx_file_xxx_list_xxx_count_xxx'").c_str());
            }
            // 3. Mysql: 删除 share_file_list
            db.Execute(("DELETE FROM share_file_list "
                        "WHERE user='"+user+"' AND md5='"+md5+
                        "' AND file_name='"+filename+"'").c_str());
            // 4. Redis
            redis.zrem(FILE_PUBLIC_ZSET, fileid);
            redis.hdel(FILE_NAME_HASH, fileid);
            return 0;
        } catch(...) {
            return 1;
        }
    }
};

// 2) SaveFile
template<>
struct ServiceMethodTraits<SaveFileRequest> {
    using ResponseType = SaveFileResponse;
    static constexpr auto Method =
        &DatabaseService::AsyncService::RequestSaveFile;
};
class SaveFileCall
    : public CallData<SaveFileRequest, SaveFileCall> {
public:
    SaveFileCall(DatabaseService::AsyncService* svc,
                 grpc::ServerCompletionQueue* cq)
      : CallData<SaveFileRequest, SaveFileCall>(svc, cq) {}
    void OnRequest(const SaveFileRequest& req,
                   SaveFileResponse& reply) {
        if (debug==1)std::cout<<"[DEBUG] SaveFileCall::OnRequest()"<<std::endl;
        reply.set_code(SyncSave(req.user(), req.md5(), req.filename()));
        rpc_finish();
    }
private:
    int SyncSave(const std::string& user,
                 const std::string& md5,
                 const std::string& filename) {
        try {
            auto& db = SakilaDatabase;
            // 1) 是否已存在
            auto r0 = db.Query(
              ("SELECT 1 FROM user_file_list WHERE user='"+user+
               "' AND md5='"+md5+"' AND file_name='"+filename+"'").c_str());
            if (r0 && r0->GetRowCount()>0) return 5;
            // 2) file_info count
            auto r1 = db.Query(
              ("SELECT count FROM file_info WHERE md5='"+md5+"'").c_str());
            if (!r1 || r1->GetRowCount()==0) return 1;
            int cnt0 = r1->Fetch()[0].GetInt32();
            db.Execute(("UPDATE file_info SET count="+std::to_string(cnt0+1)+
                        " WHERE md5='"+md5+"'").c_str());
            // 3) 插入 user_file_list
            std::string ts = _now_str();
            db.Execute((std::string("INSERT INTO user_file_list(user,md5,create_time,file_name,shared_status,pv) VALUES('")+
                        user+"','"+md5+"','"+ts+"','"+filename+"',0,0)").c_str());
            // 4) 更新或插入 user_file_count
            auto r2 = db.Query(
              ("SELECT count FROM user_file_count WHERE user='"+user+"'").c_str());
            if (!r2 || r2->GetRowCount()==0) {
                db.Execute(("INSERT INTO user_file_count(user,count) VALUES('"+user+"',1)").c_str());
            } else {
                int c2 = r2->Fetch()[0].GetInt32();
                db.Execute(("UPDATE user_file_count SET count="+std::to_string(c2+1)+
                            " WHERE user='"+user+"'").c_str());
            }
            return 0;
        } catch(...) {
            return 1;
        }
    }
};

// 3) PvShareFile
template<>
struct ServiceMethodTraits<PvShareFileRequest> {
    using ResponseType = PvShareFileResponse;
    static constexpr auto Method =
        &DatabaseService::AsyncService::RequestPvShareFile;
};
class PvShareFileCall
    : public CallData<PvShareFileRequest, PvShareFileCall> {
public:
    PvShareFileCall(DatabaseService::AsyncService* svc,
                    grpc::ServerCompletionQueue* cq)
      : CallData<PvShareFileRequest, PvShareFileCall>(svc, cq) {}
    void OnRequest(const PvShareFileRequest& req,
                   PvShareFileResponse& reply) {
        if (debug==1)std::cout<<"[DEBUG] PvShareFileCall::OnRequest()"<<std::endl;
        reply.set_code(SyncPv(req.user(), req.md5(), req.filename()));
        rpc_finish();
    }
private:
    int SyncPv(const std::string& user,
               const std::string& md5,
               const std::string& filename) {
        try {
            auto& db    = SakilaDatabase;
            auto& redis = get_redis();
            std::string fileid = md5 + filename;
            // 1) MYSQL: share_file_list pv++
            auto r0 = db.Query(
              ("SELECT pv FROM share_file_list WHERE md5='"+md5+
               "' AND file_name='"+filename+"'").c_str());
            if (!r0 || r0->GetRowCount()==0) return 1;
            int pv0 = r0->Fetch()[0].GetInt32();
            db.Execute(("UPDATE share_file_list SET pv="+std::to_string(pv0+1)+
                        " WHERE md5='"+md5+"' AND file_name='"+filename+"'").c_str());
            // 2) Redis: zscore / zincrby or zadd+hset
            auto sc = redis.zscore(FILE_PUBLIC_ZSET, fileid);
            if (sc.has_value()) {
                redis.zincrby(FILE_PUBLIC_ZSET, 1, fileid);
            } else {
                redis.zadd(FILE_PUBLIC_ZSET, fileid, pv0+1);
                redis.hset(FILE_NAME_HASH, fileid, filename);
            }
            return 0;
        } catch(...) {
            return 1;
        }
    }
};
