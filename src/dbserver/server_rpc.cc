
#include "dbapi/grpc_service.h"
#include "Global.h" 


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

    TC_LOG_INFO("", "SakilaDatabase connect & prepare statements OK");

    MySqlRpcServer rpc_server{};
    rpc_server.Run(Global::Instance().get<std::string>("Mysql_Rpc_Server"));
    
    SakilaDatabase.Close();
    MySQL::Library_End();
    return 0;
}
