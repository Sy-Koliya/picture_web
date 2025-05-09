#include "HttpConn.h"

int main(){
    std::vector<std::string> parts = {

        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: text/plain\r\n"
        "Content-L",

        "ength: 13\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "He",

        "llo, ",
  
        "world!",
        "GET /",
        "test HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "User-Agent: Mozilla/5.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Host: 127.0.0.1\r\n"
        "\r\n"
    };
    // HttpConn *ptr =new HttpConn{};
    // for(int i=0;i<parts.size();i++){     //注意，下面的权限可能已经无法访问！
    //     buffer_add(ptr->in_buf,(char*)parts[i].c_str(),parts[i].size());
    //     ptr->HandleRead();
    // }

}