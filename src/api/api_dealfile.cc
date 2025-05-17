#include "api_dealfile.h"
#include "common_api.h"
#include "RedisClient.h"
#include <nlohmann/json.hpp>
#include "grpcpp/grpcpp.h"
#include "mysql_rpc.grpc.pb.h"
#include "fdfs_rpc.grpc.pb.h"
#include "RpcCoroutine.h"
#include <mutex>

using json = nlohmann::json;
using rpc::DatabaseService;
using rpc::DeleteFileRequest;
using rpc::DeleteFileResponse;
using rpc::FdfsDeleteRequest;
using rpc::FdfsDeleteResponse;
using rpc::PvFileRequest;
using rpc::PvFileResponse;
using rpc::ShareFileRequest;
using rpc::ShareFileResponse;

static constexpr int debug = 1;

static std::unique_ptr<GrpcClient<DatabaseService, ShareFileRequest, ShareFileResponse>>
    db_share_client;
static std::unique_ptr<GrpcClient<DatabaseService, DeleteFileRequest, DeleteFileResponse>>
    db_del_client;
static std::unique_ptr<GrpcClient<DatabaseService, PvFileRequest, PvFileResponse>>
    db_pv_client;
static std::unique_ptr<GrpcClient<rpc::FdfsService, FdfsDeleteRequest, FdfsDeleteResponse>>
    fdfs_del_client;

static std::once_flag grpc_init_flag;

static void init_clients()
{
    auto db_addr = Global::Instance().get<std::string>("Mysql_Rpc_Server");
    auto fdfs_addr = Global::Instance().get<std::string>("Fdfs_Rpc_Server");
    auto db_chan = grpc::CreateChannel(db_addr, grpc::InsecureChannelCredentials());
    db_share_client.reset(new GrpcClient<DatabaseService, ShareFileRequest, ShareFileResponse>(db_chan));
    db_del_client.reset(new GrpcClient<DatabaseService, DeleteFileRequest, DeleteFileResponse>(db_chan));
    db_pv_client.reset(new GrpcClient<DatabaseService, PvFileRequest, PvFileResponse>(db_chan));
    auto fdfs_chan = grpc::CreateChannel(fdfs_addr, grpc::InsecureChannelCredentials());
    fdfs_del_client.reset(new GrpcClient<rpc::FdfsService, FdfsDeleteRequest, FdfsDeleteResponse>(fdfs_chan));
}

static int decodeDealfileJson(const std::string &post_data,
                              std::string &user,
                              std::string &token,
                              std::string &md5,
                              std::string &filename)
{
    json in;
    try
    {
        in = json::parse(post_data);
    }
    catch (const json::parse_error &e)
    {
        std::cout << "decodeDealfileJson parse error: " << e.what();
        return -1;
    }
    if (!in.contains("user")  ||
        !in.contains("token")  ||
        !in.contains("md5")    ||
        !in.contains("filename") )
    {
        std::cout << "decodeDealfileJson missing/invalid fields";
        return -1;
    }
    user = in["user"].get<std::string>();
    token = in["token"].get<std::string>();
    md5 = in["md5"].get<std::string>();
    filename = in["filename"].get<std::string>();
    return 0;
}

RpcTask<int> ApiDealfile(int fd,
                         const std::string &post_data,
                         const std::string &url)
{
    // parse out cmd, user, token, md5, filename
    std::string cmd, user, token, md5, filename;
    int code = 1;
    if (decodeDealfileJson(post_data, user, token, md5, filename) != 0)
    {
        goto RESP;
    }
    if (!QueryParseKeyValue(url, "cmd", cmd))
    {
        if (debug == 1)
            std::cout << "url parser error: " << '\n';
        goto RESP;
    }

    std::call_once(grpc_init_flag, init_clients);

    if (cmd == "share")
    {
    }
    else if (cmd == "del")
    {
        if (debug == 1)
            std::cout << "del file" << '\n';
        DeleteFileRequest req;
        req.set_user(user);
        req.set_token(token);
        req.set_md5(md5);
        req.set_filename(filename);
        DeleteFileResponse resp;
        try
        {
            if(debug == 1)
                std::cout << "rpc start" << '\n';
            resp = co_await MysqlDeleteFile(db_del_client.get(), std::move(req));
            code = resp.code(); // 0成功 1 失败 2需要成功且需要从fdfs中删除
            if(debug==1){
                std::cout<<code<<'\n';
                std::cout << "rpc end" << '\n';
            }
        }
        catch (...)
        {
            // grpc error;
            throw std::runtime_error("api_dealfile del grpc fail!");
        }
        if (code == 2)
        {
            if (debug == 1)
                std::cout << "del fdfs file" << '\n';
            FdfsDeleteRequest f_req;
            f_req.set_fileid(resp.file_id());
            FdfsDeleteResponse f_resp;
            try
            {
                f_resp = co_await FdfslDeleteFileCall(fdfs_del_client.get(), std::move(f_req));
            }
            catch (...)
            {
                // grpc error;
                throw std::runtime_error("api_dealfile del fdfs grpc fail!");
            }
            if (!f_resp.success())
            {
                code = 1;
                std::cout << "fdfs delete file error" << '\n';
            }
            else
            {
                code = 0;
            }
        }
    }
    else if (cmd == "pv")
    {
    }

RESP:
    json ht_resp;
    ht_resp["code"] = code;
    if (!SetRespToHttpConn(fd, ht_resp.dump()))
    {
        if (Global::Instance().get<int>("Debug") & Debug_std)
        {
            std::cout << "[API_DEALFILE] SetRespToHttp Error" << '\n';
        }
    }
    co_return 1;
}

