#include "ThrdPool.h"
#include "tools.h"
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
#include "memleak.h"
#include <csignal>
#include <atomic>

void fibo(int &a ,int &b){
    int res = a +b;
    a=b;
    b=res;
    std::cout<<res<<'\n';
}
static volatile std::sig_atomic_t _stop = 0;

// 信号处理器，只做一件事：标志置位
void sigint_handler(int /*signum*/) {
    _stop = 1;
}

int main(){
    std::signal(SIGINT, sigint_handler);
    int a=1,b=1;
    std::function<void()>callback = Package2FVV(fibo,a,b);
    std::function<void()>callback2 = Package2FVV(fibo,3,5);

    TimerEvent* tem= TimerEventManager::Instance().Create(callback,1000,2);
    SocketPool::Instance().AddTimerEvent(tem);

    BaseSocket* server = BaseSocketManager::Instance().Create();
    if (server->Listen("0.0.0.0", 9000) != NETLIB_OK) {
        std::cerr << "Failed to listen on 9000\n";
        return 1;
    }
    while(!_stop);
    //std::this_thread::sleep_for(std::chrono::seconds(5));
    
}