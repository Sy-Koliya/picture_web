#include "HttpServer.h"
#include "Global.h"
#include "ThrdPool.h"

// 单例指针，用于信号处理
HttpServer *HttpServer::instance = nullptr;

HttpServer &HttpServer::Instance()
{
    static HttpServer instance = HttpServer{};
    return instance;
}

HttpServer::HttpServer()
{
    instance = this;
    isptr=false;
    // 注册 SIGINT 处理函数
   // std::signal(SIGINT, HttpServer::handle_sigint);
}

HttpServer::~HttpServer() {
    std::lock_guard lk(m_lock);
    for (auto &p : conn_map) {
        p.first->Close();
    }
}


void HttpServer::handle_sigint(int)
{
    if (instance)
    {
        if (Global::Instance().get<int>("Debug") & Debug_std)
            std::cout << "SIGINT received, stopping server..." << std::endl;
        instance->stop();
    }
    else
    {
        if (Global::Instance().get<int>("Debug") & Debug_std)
            std::cout << "SIGINT received, but instance is null." << std::endl;
            _exit(EXIT_SUCCESS);
    }
}

int HttpServer::start(const char *ip, uint16_t port)
{
    // 启动监听
    if (Listen(ip, port) == NETLIB_ERROR)
        return NETLIB_ERROR;
    running = true;
    return NETLIB_OK;
}

int HttpServer::start(uint16_t port)
{
    return start("0.0.0.0", port);
}

void HttpServer::stop()
{
    {
        std::lock_guard<std::mutex> lk(mtx);
        running = false;
    }
    cv.notify_one(); // 叫醒 run() 退出
}

BaseSocket* HttpServer::AddNew_imp() {
    HttpConn* conn = new HttpConn();
    std::lock_guard lk(m_lock);
    auto expire = std::chrono::steady_clock::now() + Global::Instance().get<std::chrono::seconds>("Http_ttl_s");
    conn_map[conn] = expire;
    conn_heap.push({expire, conn});
    return conn;
}

void HttpServer::TouchConnection(HttpConn* hc) {
    std::lock_guard lk(m_lock);
    auto expire = std::chrono::steady_clock::now() + Global::Instance().get<std::chrono::seconds>("Http_ttl_s");
    conn_map[hc] = expire;
    conn_heap.push({expire, hc});
}


void HttpServer::RemoveFromConns(HttpConn* hc) {
    std::lock_guard lk(m_lock);
    conn_map.erase(hc);
}


void HttpServer::CheckTimeOutConn() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lk(m_lock);
    while (!conn_heap.empty()) {
        auto [expire, conn] = conn_heap.top();
        // 懒删除：堆顶条目失效或时间未到
        auto it = conn_map.find(conn);
        if (it == conn_map.end() || it->second != expire) {
            conn_heap.pop();
            continue;
        }
        if (expire <= now) {
            conn->Close();
            conn_map.erase(it);
            conn_heap.pop();
        } else {
            break;
        }
    }
}
int HttpServer::Close_imp() {
    TimerEventManager::Instance().Destroy(te_handle);
    std::lock_guard lk(m_lock);
    for (auto &p : conn_map) {
        p.first->Close();
    }
    conn_map.clear();
    return 0;
}


void HttpServer::loop()
{
    te_handle= TimerEventManager::Instance().Create(Package2FVV(&HttpServer::CheckTimeOutConn, this), 1000);
    std::unique_lock<std::mutex> lk(mtx);
    while (running)
    {
        cv.wait(lk, [&](){
            return !running;
        });
    }
    Close();
}
