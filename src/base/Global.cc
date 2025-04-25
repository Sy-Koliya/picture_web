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
}