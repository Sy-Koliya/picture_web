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
    friend struct ConnCompare;
    int server_socket;
    HttpState state;
    std::chrono::steady_clock::time_point last_recv;
    std::string  recv_str;
    //std::function<>  此处为了扩展性，可以在callback调用状态调用func，为了方便我使用dispatch函数
    httpparser::Request req;  //解析之后传给api
    httpparser::Response resp ;    //api处理完之后返回
};



class _500_HttpConn:public HttpConn{};

class HttpManager:public NoCopy{};


#endif