#pragma once
#include <string>
#include "RpcCoroutine.h"
using std::string;

RpcTask<int> ApiSharefiles(int fd,
                           const std::string &post_data,
                           const std::string &url);