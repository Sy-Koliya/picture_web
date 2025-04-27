#include "dbapi/commom.h"
#include "GrpcServer.h"

int main(){
    MySQL::Library_Init();

    DatabaseLoader loader;
    loader.AddDatabase(
      SakilaDatabase,
      "127.0.0.1;3306;root;js2004521;tuchuang",
      8, 2
    );

 
    if (!loader.Load()) {
        TC_LOG_ERROR("", "SakilaDatabase connect error");
        return 1;
    }
    // 到这里，所有在 SakilaDatabase 上注册的语句都已经被预编译完成

    TC_LOG_INFO("", "SakilaDatabase connect & prepare statements OK");

    RpcServer rpc_server;
    rpc_server.Run();

    SakilaDatabase.Close();
    MySQL::Library_End();
    return 0;
}
