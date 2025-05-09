#ifndef API_INSTANT_UPLOAD_H
#define API_INSTANT_UPLOAD_H

#include "RpcCoroutine.h"              
#include "mysql_rpc.grpc.pb.h"    
#include <string>
using std::string;

RpcTask<int> ApiInstantUpload(int fd, const string &post_data,const string&/*uri*/); 



#endif