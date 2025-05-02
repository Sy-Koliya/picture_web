#ifndef _API_LOGIN_H_
#define _API_LOGIN_H_

#include "RpcCoroutine.h"              
#include "mysql_rpc.grpc.pb.h"    
#include <string>


RpcTask<int> ApiMd5(int fd, const std::string& post_data); 



#endif