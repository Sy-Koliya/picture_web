// // FdfsConnectionPool.cpp
// #include "FdfsConnPool.h"
// #include "Global.h"
// #include <iostream>

// FdfsConnectionPool &FdfsConnectionPool::Instance()
// {
//     static FdfsConnectionPool init(Global::Instance().get<std::string>("s_dfs_path_client"),
//                                    Global::Instance().get<size_t>("Fdfs_ConnPool_size"));
//     return init;
// };

// FdfsConnectionPool::FdfsConnectionPool(const std::string &conf_file,
//                                        size_t pool_size)
//     : conf_file_(conf_file), pool_size_(pool_size)
// {
//     //  初始化 FastDFS 客户端
//     if (fdfs_client_init(conf_file.c_str()) != 0)
//     {
//         throw std::runtime_error("FastDFS client init failed: " + conf_file);
//     }

//     // 预分配所有连接
//     for (size_t i = 0; i < pool_size_; ++i)
//     {
//         auto conn = std::make_unique<Connection>();
//         // 连接 Tracker
//         conn->trackerConn = tracker_get_connection() if (conn->trackerConn == nullptr)
//         {
//             fdfs_client_destroy();
//             throw std::runtime_error("tracker_get_connection failed");
//         }
//         // 查询 Storage 并保存 group
//         if (tracker_query_storage_store(&conn->trackerConn,
//                                         &conn->storageConn,
//                                         conn->group,
//                                         0) != 0)
//         {
//             tracker_disconnect_server(&conn->trackerConn);
//             fdfs_client_destroy();
//             throw std::runtime_error("tracker_query_storage_store failed");
//         }

//         pool_.push(std::move(conn));
//     }
// }

// FdfsConnectionPool::~FdfsConnectionPool()
// {
//     {
//         // 通知所有等待线程：开始析构
//         std::lock_guard lock(mtx_);
//         shutting_down_ = true;
//         cv_.notify_all();
//     }

//     // 清空队列并断开连接
//     while (!pool_.empty())
//     {
//         auto conn = std::move(pool_.front());
//         pool_.pop();
//         tracker_disconnect_server(&conn->trackerConn);
//         storage_disconnect_server(&conn->storageConn);
//     }

//     fdfs_client_destroy();
// }

// std::unique_ptr<FdfsConnectionPool::Connection>
// FdfsConnectionPool::acquire()
// {
//     std::unique_lock lock(mtx_);
//     cv_.wait(lock, [this]
//              { return !pool_.empty() || shutting_down_; });
//     if (shutting_down_)
//         return nullptr;
//     auto conn = std::move(pool_.front());
//     pool_.pop();
//     return conn;
// }

// void FdfsConnectionPool::release(std::unique_ptr<Connection> conn)
// {
//     {
//         std::lock_guard lock(mtx_);
//         pool_.push(std::move(conn));
//     }
//     cv_.notify_one();
// }

// std::pair<std::string, std::string>
// FdfsConnectionPool::upload(const std::string &local_path)
// {
//     auto conn = acquire();
//     if (!conn)
//         throw std::runtime_error("upload failed: pool shutting down");

//     char fileid[FDFS_FILE_ID_MAX_LEN + 1] = {0};
//     int ret = storage_upload_by_filename(
//         &conn->trackerConn, // 1. Tracker 连接
//         &conn->storageConn, // 2. Storage 连接
//         0,                  // 3. 存储路径索引（0 表示第一个路径）
//         local_path.c_str(), // 4. 本地文件路径
//         nullptr,            // 5. 文件扩展名（不需要时传 nullptr）
//         nullptr,            // 6. 元数据列表（不需要时传 nullptr）
//         0,                  // 7. 元数据数量
//         conn->group,        // 8. 组名（之前 query_storage_store 填充的）
//         fileid              // 9. 输出的 fileid 缓冲区
//     );

//     if (ret == -1)

//         std::cerr << "[ERROR] upload returned -1 (connection?) for file: "
//                   << local_path << std::endl;

//     // 1) 关闭旧的 Storage 连接
//     storage_disconnect_server(&conn->storageConn);

//     // 2) 重新向 Tracker 申请新的 Storage
//     if (tracker_query_storage_store(&conn->trackerConn,
//                                     &conn->storageConn,
//                                     conn->group,
//                                     0) != 0)
//     {
//         std::cerr << "[ERROR] failed to re-establish storage connection"
//                   << " for file: " << local_path << std::endl;
//         release(std::move(conn));
//         throw std::runtime_error("upload failed: cannot reconnect storage");
//     }
//     std::cerr << "[INFO] re-established storage connection, retrying upload"
//               << std::endl;

//     // 3) 再次尝试上传
//     int ret = storage_upload_by_filename(
//     &conn->trackerConn,
//     &conn->storageConn,
//     0,
//     local_path.c_str(),
//     nullptr,
//     nullptr,
//     0,
//     conn->group,
//     fileid);
//     }

// // 如果第二次调用仍失败，或第一次调用返回其他非零错误
// if (ret != 0)
// {
//     std::cerr << "[ERROR] upload failed (ret=" << ret
//               << ") for file: " << local_path << std::endl;
//     release(std::move(conn));
//     throw std::runtime_error("upload failed");
// }

// // 上传成功，构造 storage_addr 并归还连接
// std::string storage_addr =
//     std::string(conn->storageConn.server_ip) + ":" +
//     std::to_string(conn->storageConn.server_port);

// release(std::move(conn));
// return {std::string(fileid), storage_addr};
// }

// bool FdfsConnectionPool::remove(const std::string &fileid)
// {
//     auto conn = acquire();
//     if (!conn)
//         return false;

//     char group[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
//     char filename[256] = {0};
//     bool ok = false;

//     if (fdfs_split_file_id(fileid.c_str(), group, filename) == 0)
//     {
//         int ret = storage_delete_file1(group, filename);
//         ok = (ret == 0);
//         if (!ok)
//             std::cerr << "Delete failed: "<< fileid << '\n';
//         else
//             std::cout << "Delete success: "<< fileid << '\n';
//     }
//     else
//     {
//         std::cerr << "Invalid fileid:"<<fileid<< << '\n';
//     }

//     release(std::move(conn));
//     return ok;
// }

// std::string FdfsConnectionPool::getUrl(const std::string &fileid,
//                                        const std::string &storage_addr) const
// {
//     return "http://" + storage_addr + "/" + fileid;
// }