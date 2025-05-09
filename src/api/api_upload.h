#pragma once
#include <string>
#include "RpcCoroutine.h"
using std::string;
// 异步上传接口，返回 UploadResponse
RpcTask<int> ApiUploadFile(int fd, const string &post_data,const string& /*uri*/);