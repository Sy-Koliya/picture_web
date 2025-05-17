#ifndef _API_LOGIN_H_
#define _API_LOGIN_H_

#include "RpcCoroutine.h"             
#include "mysql_rpc.grpc.pb.h"     
#include <string>
using std::string;



// 协程接口签名：输入 fd, HTTP body, 返回业务 code
RpcTask<int> ApiUserLogin(int fd, const string &post_data,const string& /*uri*/);



#endif