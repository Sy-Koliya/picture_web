#include "Global.h"
#include <iostream>
 
int main() {
    
    // 存储
    Global::Instance().set<int>("port", 8080);
    Global::Instance().set<std::string>("host", "127.0.0.1");

    // 读取
    int port = Global::Instance().get<int>("port");
    std::string host = Global::Instance().get<std::string>("host");
    std::cout << "Server listening on " << host << ":" << port << "\n";

    // 检查和移除
    if (Global::Instance().contains("port")) {
        Global::Instance().remove("port");
    }

    return 0;
}
