#include "HttpServer.h"
#include "Global.h"
#include "ThrdPool.h"

HttpServer *HttpServer::instance = nullptr;

HttpServer& HttpServer::Instance()
{
    static HttpServer *instance = []()
    {
        return new HttpServer();
    }();
    return *instance;
}

HttpServer::HttpServer()
{
    // 单例指针，用于信号处理
    instance = this;
    // 注册 SIGINT 处理函数
    std::signal(SIGINT, HttpServer::handle_sigint);
}

HttpServer::~HttpServer()
{
    // 清理所有连接
    for (auto &kv : conns)
    {
        kv->Close();
    }
    conns.clear();
}

void HttpServer::handle_sigint(int)
{
    if (instance)
    {
        std::cout << "SIGINT received, stopping server..." << std::endl;
        instance->stop();
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
    cv.notify_one();  // 叫醒 run() 退出
}

BaseSocket *HttpServer::AddNew_imp()
{
    // 接到新连接时由 BaseSocket 调用
    HttpConn *conn = new HttpConn();
    conn->server_socket = m_socket;
    conns.insert(conn);
    return conn;
}

void HttpServer::CheckTimeOutConn(){
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now -  Global::Instance().get<std::chrono::seconds>("Http_ttl_s");

    // 重复检查集合最前面的元素
    auto it = conns.begin();
    while (it != conns.end())
    {
        HttpConn *conn = *it;
        // 如果是 nullptr，或者最后一次接收时间早于过期阈值
        if (conn == nullptr || conn->last_recv < expire_time)
        {
            if (conn!=nullptr)
            {
                conn->Close();    
            }
            // erase 返回下一个有效迭代器
            it = conns.erase(it);
        }
        else
        {
            break;
        }
    }
}

void HttpServer::Close_Conn(){
    for(auto it :conns){
        if(it!=nullptr){
            it->Close();
        }
    }
}

void HttpServer::loop()
{
    TimerEvent* ev  = TimerEventManager::Instance().Create(Package2FVV(&HttpServer::CheckTimeOutConn,this),1000);
    SocketPool::Instance().AddTimerEvent(ev);
    std::unique_lock<std::mutex> lk(mtx);
    int loop_wait_duration_mil = Global::Instance().get<int>("loop_wait_duration_mil");
    while (running)
    {
        cv.wait_for(lk, std::chrono::milliseconds(loop_wait_duration_mil));
    }
    Close_Conn();
    Close();
}
