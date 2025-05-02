#include "Global.h"
#include <chrono>
Global::Global()
{
    vars_["WorkPool"] = (size_t)8;
    vars_["SocketPool"] = (size_t)4;
    vars_["/test"] /*= func_ptr*/;
    vars_["Content_length_type"] = std::string("Content-Length");
    vars_["Http_ttl_s"] = std::chrono::seconds(60);
    vars_["loop_wait_duration_mil"] = 100;
    vars_["Debug"] = (int)Debug_std; // 1<<1
    vars_["Mysql_Rpc_Server"]= std::string("0.0.0.0:50051");
    vars_["Redis_Host"]     = std::string("127.0.0.1");
    vars_["Redis_Port"]     = (int)6379;
    vars_["Redis_Password"] = std::string("");
    vars_["Redis_PoolSize"] = (size_t)5;
}