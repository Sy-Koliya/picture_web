#ifndef _API_REGISTER_H_
#define _API_REGISTER_H_
#include <string>
#include "RpcCoroutine.h"
using std::string;



//注册 有了就返回2 没有返回1  
 RpcTask<int>  ApiRegisterUser(int fd,string &post_data);
#endif