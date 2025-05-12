// FdfsConnectionPool.cpp
#include "FdfsConnPool.h"
#include "Global.h"
#include <iostream>
#include <memory>


#define __FDFS_FILE_BUF_LEN 128

FdfsConnectionPool &FdfsConnectionPool::Instance()
{
    static FdfsConnectionPool instance(
        Global::Instance().get<std::string>("s_dfs_path_client"),
        Global::Instance().get<size_t>("Fdfs_ConnPool_size"));
    return instance;
}

FdfsConnectionPool::FdfsConnectionPool(const std::string &conf_file,
                                       size_t pool_size)
    : conf_file_(conf_file), pool_size_(pool_size)
{
    // 初始化客户端
    if (fdfs_client_init(conf_file.c_str()) != 0)
    {
        throw std::runtime_error("FastDFS client init failed: " + conf_file);
    }
    // 预分配连接池
    try
    {
        for (size_t i = 0; i < pool_size_; ++i)
        {
            auto conn = std::make_unique<Connection>();
            // 获取tracker连接
            conn->trackerConn = tracker_get_connection();
            if (!conn->trackerConn)
            {
                throw std::runtime_error("tracker_get_connection failed");
            }

            if (tracker_query_storage_store(conn->trackerConn,
                                            &conn->storageConn,
                                            conn->group,
                                            &conn->idx) != 0)
            {
                throw std::runtime_error("tracker_query_storage_store failed");
            }

            pool_.push(std::move(conn));
        }
    }
    catch (...)
    {
        // 构造失败，清理已创建的连接
        while (!pool_.empty())
        {
            auto c = std::move(pool_.front());
            pool_.pop();
            tracker_close_connection_ex(c->trackerConn, true);
        }
        fdfs_client_destroy();
        throw;
    }
}

FdfsConnectionPool::~FdfsConnectionPool()
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        shutting_down_ = true;
        cv_.notify_all();
    }
    // 断开所有连接
    while (!pool_.empty())
    {
        auto c = std::move(pool_.front());
        pool_.pop();
        tracker_close_connection_ex(c->trackerConn, true);
    }
    fdfs_client_destroy();
}

std::unique_ptr<FdfsConnectionPool::Connection>
FdfsConnectionPool::acquire()
{
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]
             { return !pool_.empty() || shutting_down_; });
    if (shutting_down_)
        return nullptr;
    auto conn = std::move(pool_.front());
    pool_.pop();
    return conn;
}

void FdfsConnectionPool::release(std::unique_ptr<Connection> conn)
{
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool_.push(std::move(conn));
    }
    cv_.notify_one();
}

std::pair<std::string, std::string>
 FdfsConnectionPool::upload(const std::string &local_path)
 {
     auto conn = acquire();
     if (!conn)
         throw std::runtime_error("upload failed: pool shutting down");

     char fileid_buf[__FDFS_FILE_BUF_LEN] = {0};
     int ret = storage_upload_by_filename(
         conn->trackerConn,
         &conn->storageConn,
         0,
         local_path.c_str(),
         nullptr,
         nullptr,
         0,
         conn->group,
         fileid_buf);

     // 第一次失败，重连再试（保持不变）
     if (ret != 0)
     {
         std::cerr << "[WARN] upload failed(ret=" << ret << "), reconnecting\n";
         if (tracker_query_storage_store(
                 conn->trackerConn,
                 &conn->storageConn,
                 conn->group,
                 &conn->idx) != 0)
         {
             release(std::move(conn));
             throw std::runtime_error("Failed to reconnect to storage");
         }
         ret = storage_upload_by_filename(
             conn->trackerConn,
             &conn->storageConn,
             0,
             local_path.c_str(),
             nullptr,
             nullptr,
             0,
             conn->group,
             fileid_buf);
         if (ret != 0)
         {
             release(std::move(conn));
             throw std::runtime_error("Upload retry failed (ret=" + std::to_string(ret) + ")");
         }
     }


     std::string fileid_{conn->group};
        fileid_.push_back('/');
        fileid_ += fileid_buf;

    if (tracker_query_storage_update1(
            conn->trackerConn,
            &conn->storageConn,
            const_cast<char*>(fileid_.c_str())) != 0)
         {
         std::cerr << "[WARN] tracker_query_storage_update1 failed for " << fileid_ << "\n";
         }


    std::string storage_ip = conn->storageConn.ip_addr;


    std::string http_port = Global::Instance().get<std::string>("s_storage_web_server_port");


    std::string addr = "http://" + storage_ip + ":" + http_port + "/" + fileid_;

     release(std::move(conn));
     return {fileid_, addr};
 }


bool FdfsConnectionPool::remove(const std::string &fileid) {
    // 1. 从连接池取出一个 Tracker 连接
    auto conn = acquire();
    if (!conn) {
        std::cerr << "[ERROR] Remove failed: connection pool is shutting down\n";
        return false;
    }

    //  查询对应 fileid 的 Storage 服务器信息
    int ret = tracker_query_storage_update1(
        conn->trackerConn,
        &conn->storageConn,
        const_cast<char*>(fileid.c_str())  
    );
    if (ret != 0) {
        std::cerr << "[ERROR] tracker_query_storage_update1 failed, ret="
                  << ret << ", fileid=" << fileid << "\n";
        release(std::move(conn));
        return false;
    }

    ret = storage_delete_file1(
        conn->trackerConn,
        &conn->storageConn,
        fileid.c_str()                   
    );
    bool success = (ret == 0);
    if (!success) {
        std::cerr << "[ERROR] storage_delete_file1 failed, ret="
                  << ret << ", fileid=" << fileid << "\n";
    }

    // 4. 归还连接
    release(std::move(conn));
    return success;
}


std::string FdfsConnectionPool::getUrl(const std::string &fileid,
                                       const std::string &storage_addr) const
{
    return "http://" + storage_addr + "/" + fileid;
}