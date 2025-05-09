// // api_upload.cpp
// #include "api_upload.h"
// #include "RedisClient.h"
// #include "RpcCoroutine.h"
// #include "common_api.h"
// #include "Global.h"
// #include "mysql_rpc.grpc.pb.h"
// #include "fdfs_rpc.grpc.pb.h"
// #include "HttpConn.h"
// #include <grpcpp/grpcpp.h>
// #include <filesystem>
// #include <nlohmann/json.hpp>

// using grpc::ClientContext;
// using rpc::FdfsUploadRequest;
// using rpc::FdfsUploadResponse;
// using rpc::UploadRequest;
// using rpc::UploadResponse;

// static std::unique_ptr<
//     GrpcClient<rpc::DatabaseService,
//                rpc::UploadRequest,
//                rpc::UploadResponse>>
//     mysql_upload_client;

// static std::unique_ptr<
//     GrpcClient<rpc::FdfsService,
//                rpc::FdfsUploadRequest,
//                rpc::FdfsUploadResponse>>
//     fdfs_upload_client;

// static std::once_flag upload_init_flag;

// static void init_upload_client()
// {
//     std::string ep = Global::Instance().get<std::string>("Mysql_Rpc_Server");
//     auto channel = grpc::CreateChannel(ep,
//                                        grpc::InsecureChannelCredentials());
//     mysql_upload_client = std::make_unique<
//         GrpcClient<rpc::DatabaseService,
//                    rpc::UploadRequest,
//                    rpc::UploadResponse>>(channel);
//     std::string ep2 = Global::Instance().get<std::string>("Fdfs_Rpc_Server");
//     auto channel2 = grpc::CreateChannel(ep2,
//                                         grpc::InsecureChannelCredentials());
//     fdfs_upload_client = std::make_unique<
//         GrpcClient<rpc::FdfsService,
//                    rpc::FdfsUploadRequest,
//                    rpc::FdfsUploadResponse>>(channel2);
// }


// static std::string GetFileSuffix(const std::string &filename)
// {
//     auto pos = filename.rfind('.');
//     if (pos == std::string::npos || pos + 1 >= filename.size())
//     {
//         return std::string{};
//     }
//     return filename.substr(pos + 1);
// }

// // Helper to extract the value of a multipart form field
// static std::string extractField(const std::string &body, const std::string &fieldName)
// {
//     // 先找到token字段，然后一行空格，接着就是数据
//     std::string token = "name=\"" + fieldName + "\"";
//     size_t pos = body.find(token);
//     if (pos == std::string::npos)
//     {
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "Missing field: " << fieldName << '\n';
//         }
//         throw std::runtime_error("Missing form field");
//     }
//     pos = body.find("\r\n", pos);
//     pos += 4; // 跳到数据行
//     size_t end = body.find("\r\n", pos);
//     return body.substr(pos, end - pos);
// }
// // 解析multi-boundery字段
// void ParseMultipart(const std::string &post_data,
//                     std::string &local_path,
//                     std::string &filename,
//                     std::string &md5,
//                     std::string &user,
//                     uint64_t &size)
// {
//     // Parse boundary
//     size_t p1 = post_data.find("\r\n");
//     if (p1 == std::string::npos)
//     {
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "Wrong or missing boundary!" << '\n';
//         }
//         return;
//     }
//     std::string boundary = post_data.substr(0, p1);

//     if (Global::Instance().get<int>("Debug") & Debug_std)
//     {
//         std::cout << "Boundary: " << boundary << '\n';
//     }

//     try
//     {

//         filename = extractField(post_data, "file_name");
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "file_name: " << filename << '\n';
//         }

//         std::string contentType = extractField(post_data, "file_content_type");
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "file_content_type: " << contentType << '\n';
//         }

//         local_path = extractField(post_data, "file_path");
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "file_path: " << local_path << '\n';
//         }

//         md5 = extractField(post_data, "file_md5");
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "file_md5: " << md5 << '\n';
//         }

//         std::string sizeStr = extractField(post_data, "file_size");
//         size = std::stoull(sizeStr);
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "file_size: " << size << '\n';
//         }

//         user = extractField(post_data, "user");
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "user: " << user << '\n';
//         }
//     }
//     catch (const std::exception &e)
//     {
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "Failed to parse multipart data: " << e.what() << '\n';
//         }
//         throw;
//     }

//     // 修改后缀
//     std::string suffix = GetFileSuffix(filename); // e.g., "mp4", "jpg"
//     std::filesystem::path oldPath(local_path);
//     std::filesystem::path newPath = oldPath;
//     newPath.replace_extension(suffix);

//     try
//     {
//         std::filesystem::rename(oldPath, newPath);
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "Renamed " << oldPath << " -> " << newPath << '\n';
//         }
//         local_path = newPath.string();
//     }
//     catch (const std::exception &e)
//     {
//         if (Global::Instance().get<int>("Debug") & Debug_std)
//         {
//             std::cout << "Rename failed: " << e.what() << '\n';
//         }
//     }
// }
// // HTTP 文件上传处理协程
// RpcTask<int> ApiUploadFile(int fd, const string &post_data,const string& /*uri*/)
// {
//     //  解析 multipart/form-data
//     std::string local_path, filename, md5, user;
//     uint64_t size = 0;
//     try
//     {
//         ParseMultipart(post_data, local_path, filename, md5, user, size);
//     }
//     catch (const std::exception &e)
//     {
//         co_return -1;
//     }

//     std::call_once(upload_init_flag, init_upload_client);

//     int code = 1;

//     // FastDFS 上传
//     FdfsUploadRequest fdfs_req;
//     fdfs_req.set_path(local_path);
//     std::string fileid, url;
//     try
//     {
//         // 异步 RPC 调用
//         FdfsUploadResponse resp = co_await FdfslUploadFileCall(fdfs_upload_client.get(), std::move(fdfs_req));
//         fileid = resp.fileid();
//         url = resp.url();
//     }
//     catch (const std::exception &e)
//     {
//         code = 1;
//         throw std::runtime_error("Fastdfs Upload fail");
//     }

//     //  Redis 缓存
//     auto redis = get_redis();
//     redis.set(fileid, url);
//     // 初始化 gRPC 客户端

//     // 构造 RPC 请求
//     UploadRequest req;
//     req.set_user(user);
//     req.set_filename(filename);
//     req.set_file_md5(md5);
//     req.set_file_size(size);
//     req.set_fileid(fileid);
//     req.set_url(url);

//     try
//     {
//         // 异步 RPC 调用
//         UploadResponse resp = co_await MysqlUploadFileCall(mysql_upload_client.get(), std::move(req));
//         code = resp.code();
//     }
//     catch (const std::exception &e)
//     {
//         code = 1;
//     }

//     // 7. 发送 HTTP 响应
//     nlohmann::json j;
//     j["code"] = code;
//     std::string body = j.dump();

//     std::string response =
//         "HTTP/1.1 200 OK\r\n"
//         "Connection: close\r\n"
//         "Content-Type: application/json; charset=utf-8\r\n"
//         "Content-Length: " +
//         std::to_string(body.size()) +
//         "\r\n\r\n" + body;

//     if (auto *sock = FindBaseSocket(fd); sock && sock->IsAlive())
//     {
//         if (auto *h = dynamic_cast<HttpConn *>(sock))
//         {
//             h->SetResponse(std::move(response));
//         }
//     }

//     // 8. 清理本地文件
//     std::remove(local_path.c_str());

//     co_return code;
// }

// /*
// OnRead, buf_len=1321, conn_handle=2, POST /api/upload HTTP/1.0
// Host: 127.0.0.1:8081
// Connection: close
// Content-Length: 722
// Accept: application/json, text/plain,
// User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36 Edg/106.0.1370.52
// Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryjWE3qXXORSg2hZiB
// Origin: http://114.215.169.66
// Referer: http://114.215.169.66/myFiles
// Accept-Encoding: gzip, deflate
// Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
// Cookie: userName=qingfuliao; token=e4252ae6e49176d51a5e87b41b6b9312

// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="file_name"

// config.ini
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="file_content_type"

// application/octet-stream
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="file_path"

// /root/tmp/5/0034880075
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="file_md5"

// 10f06f4707e9d108e9a9838de0f8ee33
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="file_size"

// 20
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB
// Content-Disposition: form-data; name="user"

// qingfuliao
// ------WebKitFormBoundaryjWE3qXXORSg2hZiB--
// */
// // 
