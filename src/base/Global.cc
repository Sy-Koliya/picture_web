#include "Global.h"

Global::Global(){
    vars_["WorkPool"] = (size_t)8;
    vars_["SocketPool"] = (size_t)4;
    vars_["/test"]/*= func_ptr*/;
    vars_["Content_length_type"]=std::string("Content-Length");
}