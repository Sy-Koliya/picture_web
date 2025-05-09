#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "BaseSocket.h"
#include "HttpConn.h"
#include <atomic>
#include <set>
#include <csignal>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <chrono>


struct ConnEntry {
    std::chrono::steady_clock::time_point expire;
    HttpConn* conn;
};



struct MinHeapCompare {
    bool operator()(ConnEntry const &a, ConnEntry const &b) const noexcept {
        return a.expire > b.expire;
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
    friend class HttpConn;
    void RemoveFromConns(HttpConn* hc);
    void TouchConnection(HttpConn* hc);
private:
    HttpServer();
    ~HttpServer();
private:
    BaseSocket *AddNew_imp() override;
private:
    void CheckTimeOutConn();
    int Close_imp() override;
private:
    std::atomic<bool> running{false};


    std::unordered_map<HttpConn*, std::chrono::steady_clock::time_point> conn_map;
    std::priority_queue<ConnEntry, std::vector<ConnEntry>, MinHeapCompare> conn_heap;

    static HttpServer *instance;
    static void handle_sigint(int signo);

    std::mutex               mtx;
    std::mutex               m_lock;
    std::condition_variable  cv;
    int te_handle;
};

#endif