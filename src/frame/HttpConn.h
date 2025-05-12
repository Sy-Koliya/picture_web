#ifndef HTTPCONN_H
#define HTTPCONN_H

#include "BaseSocket.h"
#include "types.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <mutex>

constexpr int  recv_buf_len = 1024;
constexpr int  send_buf_len = 1024;
constexpr int  max_recv_len = 4096;

//HttpServer -> Listen -> HttpConn  Read ->解析完 -> HttpRequest ->api ->HttpResponce ->HttpConn  Write ->send  
// api 可以通过 BaseFound(fd)找到原来的 HttpConn  
class HttpConn:public BaseSocket{

public: 
    HttpState GetState(){return state;}
    HttpConn();
    ~HttpConn();
    int  SetResponse(std::string &&_resp);
    int  SetErrorResponse(int code,const std::string& reason={});
private:
    int Read_imp() override;
    int Write_imp() override;
    int Connect_imp() override;
    int Close_imp() override; 

public:
    void HandleRead();

private:
    friend class HttpServer;
    friend struct ConnCompare;
    std::mutex h_lock;
    HttpState state;
    std::chrono::steady_clock::time_point last_recv;
    std::string  recv_str;
    bool isKeepAlive;
    httpparser::Request req;  //解析之后传给api
    std::string resp ;    //api处理完之后返回
    int sended_size;
};



class _500_HttpConn:public HttpConn{};

class HttpManager:public NoCopy{};


#endif