// FdfsConnPool.h
#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <stdexcept>
extern "C" {
    #include <fastdfs/fdfs_client.h>   
    #include <fastdfs/tracker_client.h>
    #include <fastdfs/storage_client.h>
    #undef byte      
    #undef trim      
}


class FdfsConnectionPool
{
public:
    /**
     * @param conf_file    FastDFS 客户端配置文件路径
     * @param pool_size    连接池大小
     * @throws std::runtime_error 如果初始化或预分配连接失败
     */
    static FdfsConnectionPool& Instance();

    /**
     * 上传文件
     * @param local_path 本地文件路径
     * @return 上传成功后返回 (fileid,storage_http)，否则抛异常
     */
    std::pair<std::string, std::string> upload(const std::string& local_path);

    /**
     * 删除文件
     * @param fileid 要删除的 fileid
     * @return 成功返回 true，失败返回 false
     */
    bool remove(const std::string& fileid);

   
private:
     /**
     * @brief fileid 生成完整的 HTTP 访问 URL
     */
    std::string getUrl(const std::string& fileid,
        const std::string& storage_addr) const;


    
    FdfsConnectionPool(const std::string& conf_file,
            size_t pool_size);

    ~FdfsConnectionPool();

    struct Connection {
        ConnectionInfo *trackerConn;
        ConnectionInfo storageConn;  
        char group[FDFS_GROUP_NAME_MAX_LEN + 1];
      };
      

    const std::string conf_file_;
    const size_t      pool_size_;

    mutable std::mutex                         mtx_;
    std::condition_variable                    cv_;
    std::queue<std::unique_ptr<Connection>>    pool_;
    bool                                       shutting_down_ = false;

    // 从池中取出一个连接；如果正在析构，则返回 nullptr
    std::unique_ptr<Connection> acquire();

    // 将连接放回池中
    void release(std::unique_ptr<Connection> conn);
};
