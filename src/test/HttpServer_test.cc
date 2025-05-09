#include "HttpServer.h"
//#include "memleak.h"

int main(){
    try {
  //      init_hook();
        HttpServer::Instance().start(8081);
        HttpServer::Instance().loop();
      } catch (const std::exception& e) {
        std::cerr<<"主函数捕获异常: "<<e.what()<<"\n";
        return 1;
      } catch (...) {
        std::cerr<<"主函数捕获未知异常\n";
        return 1;
      }
}