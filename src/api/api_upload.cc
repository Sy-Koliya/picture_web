


/* ApiUpload.cpp */
#include "api_upload.h"

#include <sw/redis++/redis++.h>  // redis_plus_plus
#include "GrpcClient.h"          // for DB RPC client

using namespace std;
using namespace sw::redis;

// Global config
static string g_fdfs_conf;
static string g_storage_host;
static uint16_t g_storage_port;

// Initialize upload service
int ApiUploadInit(const string &fdfs_conf,
                  const string &storage_host,
                  uint16_t storage_port) {
    g_fdfs_conf = fdfs_conf;
    g_storage_host = storage_host;
    g_storage_port = storage_port;
    return 0;
}

// Helpers for FastDFS
static string uploadToFastDFS(const string &local_path) {
    // file_id buffer
    char file_id[256] = {0};
    int ret = fdfs_upload_by_filename1(g_fdfs_conf.c_str(),
                                       local_path.c_str(),
                                       file_id, sizeof(file_id));
    if (ret != 0) {
        throw runtime_error("FastDFS upload error: " + string(fdfs_get_error_info(ret)));
    }
    return string(file_id);
}

static string makeFileUrl(const string &file_id) {
    ostringstream oss;
    oss << "http://" << g_storage_host << ":" << g_storage_port << "/" << file_id;
    return oss.str();
}

// Simple form-data parser (fields are assumed in order and small)
static unordered_map<string,string> parseForm(const string &data) {
    unordered_map<string,string> fields;
    size_t pos = 0, end;
    // Extract boundary
    end = data.find("\r\n");
    string boundary = data.substr(0, end);
    string token;
    while ((pos = data.find(boundary, pos)) != string::npos) {
        // skip boundary + \r\n
        pos += boundary.size() + 2;
        // header
        auto name_pos = data.find("name=\"", pos) + 6;
        auto name_end = data.find('"', name_pos);
        string name = data.substr(name_pos, name_end - name_pos);
        // skip to content
        auto val_start = data.find("\r\n\r\n", name_end) + 4;
        auto val_end = data.find("\r\n", val_start);
        string value = data.substr(val_start, val_end - val_start);
        fields[name] = value;
        pos = val_end;
    }
    return fields;
}

RpcTask<int> ApiUpload(int fd, const string &post_data, string &str_json) {
    Json::Value resp;
    try {
        // 1. Parse form
        auto fields = parseForm(post_data);
        auto file_name = fields.at("file_name");
        auto content_type = fields.at("file_content_type");
        auto tmp_path = fields.at("file_path");
        auto md5 = fields.at("file_md5");
        auto file_size = stoll(fields.at("file_size"));
        auto user = fields.at("user");

        // 2. Rename temp file with proper suffix
        filesystem::path oldp(tmp_path);
        string suffix = filesystem::path(file_name).extension().string();
        filesystem::path newp = oldp;
        newp.replace_extension(suffix);
        filesystem::rename(oldp, newp);

        // 3. Upload to FastDFS
        auto file_id = uploadToFastDFS(newp.string());
        auto file_url = makeFileUrl(file_id);
        spdlog::info("Uploaded {} -> {}", newp.string(), file_id);

        // 4. Store metadata via RPC to DB server
        // Prepare RPC request
        rpc::UploadRequest db_req;
        db_req.set_user(user);
        db_req.set_filename(file_name);
        db_req.set_md5(md5);
        db_req.set_size(file_size);
        db_req.set_file_id(file_id);
        db_req.set_url(file_url);
        
        // Use coroutine-based gRPC client
        auto channel = grpc::CreateChannel("dbserver:50052", grpc::InsecureChannelCredentials());
        MysqlClient<rpc::UploadRequest, rpc::UploadResponse> client(channel);
        auto db_resp = co_await client.make_awaitable<&rpc::FileService::Stub::AsyncUploadFile>(db_req);
        if (db_resp.code() != 0) {
            throw runtime_error("DB store failed, code=" + to_string(db_resp.code()));
        }

        // 5. Clean up
        filesystem::remove(newp);
        fields.clear();

        // 6. Reply
        resp["code"] = 0;
        resp["url"]  = file_url;
        str_json = resp.toStyledString();
        co_return 0;
    } catch (const exception &ex) {
        spdlog::error("ApiUpload error: {}", ex.what());
        resp["code"] = 1;
        resp["message"] = ex.what();
        str_json = resp.toStyledString();
        co_return -1;
    }
}
