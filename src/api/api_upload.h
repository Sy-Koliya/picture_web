#pragma once

#include <string>
#include <fdfs_client.h>  // FastDFS client API
#include <tracker_client.h>
#include "RpcCoroutine.h"

RpcTask<int> ApiUpload(int fd, const std::string &post_data);