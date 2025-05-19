
#pragma once
#include <string>
#include "RpcCoroutine.h"

RpcTask<int> ApiSharepicture(int fd,
                             const std::string &post_data,
                             const std::string &url);