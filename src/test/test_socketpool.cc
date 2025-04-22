#include "ThrdPool.h"
#include "tools.h"
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>
void fibo(int &a ,int &b){
    int res = a +b;
    a=b;
    b=res;
    std::cout<<res<<'\n';
}
void echo_callback(void* /* callback_data */, uint8_t msg, uint32_t handle, void* /* pParam */) {
    // 通过句柄查找我们的 BaseSocket，并为其引用计数 +1
    BaseSocket* sock = FindBaseSocket(handle);
    if (!sock) return;

    if (msg == NETLIB_MSG_READ) {
        // 客户端有数据可读
        char buf[4096];
        int ret = sock->Recv(buf, sizeof(buf));
        if (ret > 0) {
            // 收到 ret 字节，就原样发回去
            sock->Send(buf, ret);
        }
        else if (ret == 0) {
            // 对端关闭
            sock->Close();
        }
    }
    else if (msg == NETLIB_MSG_CLOSE) {
        // 连接被关闭
        sock->Close();
    }
    else if (msg == NETLIB_MSG_CONFIRM) {
        // 非阻塞 connect 完成
        std::cout << "New connection established: fd=" << handle << "\n";
    }

    // 与 FindBaseSocket 对应，释放引用
}

int main(){
    int a=1,b=1;
    std::function<void()>callback = Package2FVV(fibo,a,b);
    std::function<void()>callback2 = Package2FVV(fibo,3,5);

    //下面这两句可以包装成一个工厂函数，避免用户拿到指针，造成不安全的问题
    TimerEvent* tem= TimerEventManager::Instance().createEvent(callback,1000,2);
    SocketPool::Instance().AddTimerEvent(tem);

    BaseSocket* server = new BaseSocket();
    if (server->Listen("0.0.0.0", 9000, echo_callback, nullptr) != NETLIB_OK) {
        std::cerr << "Failed to listen on 9000\n";
        return 1;
    }
    //SocketPool::Instance().AddSocketEvent(server->GetSocket(), SOCKET_READ );
    while(true);

}