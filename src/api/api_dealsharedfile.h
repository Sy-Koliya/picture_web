#pragma once
#include <string>
#include "RpcCoroutine.h"
using std::string;
// 异步上传接口，返回 UploadResponse

RpcTask<int> ApiDealsharefile(int fd,
                         const std::string &post_data,
                         const std::string &url);