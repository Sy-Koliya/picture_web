#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "BaseSocket.h"
#include "HttpConn.h"
#include <atomic>
#include <set>
#include <csignal>
#include <mutex>
#include <condition_variable>


struct ConnCompare
{
    bool operator()(const HttpConn *a, const HttpConn *b) const noexcept
    {
        // 相同指针或都为空
        if (a == b) return false;  

        // nullptr 始终最前
        if (a == nullptr) return true;
        if (b == nullptr) return false;

        // 按 last_recv 升序
        if (a->last_recv < b->last_recv) return true;
        if (b->last_recv < a->last_recv) return false;

        // 如果时间相同，退而求其次按地址排序，保证严格弱序
        return a < b;
    }
};

class HttpServer : public BaseSocket,public NoCopy
{

public:
    static HttpServer& Instance();

    int start(const char *ip, const uint16_t port);
    int start(uint16_t port);
    void loop(); // 事件循环启动
    void stop();
private:
    HttpServer();
    ~HttpServer();
private:
    BaseSocket *AddNew_imp() override;
private:
    void CheckTimeOutConn();
    void Close_Conn();
private:
    friend  int HttpConn::Close_imp();
    std::atomic<bool> running{false};
    std::set< HttpConn *,ConnCompare> conns; 

    static HttpServer *instance;
    static void handle_sigint(int signo);

    std::mutex               mtx;
    std::condition_variable  cv;
};

#endif