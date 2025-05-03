#ifndef _API_LOGIN_H_
#define _API_LOGIN_H_

#include "RpcCoroutine.h"               // 定义 RpcTask<T> & co_await 支撑
#include "mysql_rpc.grpc.pb.h"     // grpc 生成的 LoginRequest/Response, DatabaseService
#include <string>




// 协程接口签名：输入 fd, HTTP body, 返回业务 code
RpcTask<int> ApiUserLogin(int fd, const std::string &post_data);



#endif