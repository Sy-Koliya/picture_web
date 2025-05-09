#include "api_register.h"
#include "nlohmann/json.hpp"
#include "CoroutineCompeleteQueue.h"
#include "Api_dispatch.h"

int main(){

    nlohmann::json j;
    j["nickName"] = "小明";
    j["userName"] = "xiaoming";
    j["firstPwd"] = "daodoahdo";
    std::cout << j.dump(2, ' ', true) << '\n';
    std::string str = j.dump();
    WorkPool::Instance().Submit( &api_dispatch,4,"/api/reg",str);

    while(true);
}