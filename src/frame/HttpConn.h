#ifndef HTTPCONN_H
#define HTTPCONN_H

#include "BaseSocket.h"
#include "types.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

constexpr int  recv_buf_len = 1024;
constexpr int  max_recv_len = 4096;

//HttpServer -> Listen -> HttpConn  Read ->解析完 -> HttpRequest ->api ->HttpResponce ->HttpConn  Write ->send  
// api 可以通过 BaseFound(fd)找到原来的 HttpConn  
class HttpConn:public BaseSocket{

public: 
    HttpState GetState(){return state;}
    HttpConn();
    ~HttpConn();
private:
    int Read_imp() override;
    int Write_imp() override;
    int Connect_imp() override;
    int Close_imp() override; //not found 处理

public:
    void HandleRead();

private:
    friend class HttpServer;
    HttpState state;
    std::chrono::steady_clock::time_point last_recv;
    std::string  recv_str;
    httpparser::Request req;  //解析之后传给api
    httpparser::Response resp ;    //api处理完之后返回
};



class _500_HttpConn:public HttpConn{};

class HttpManager:public NoCopy{};


#endif