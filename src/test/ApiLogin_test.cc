#include "api_login.h"
#include "nlohmann/json.hpp"
#include "CoroutineCompeleteQueue.h"

int main(){
    {
    nlohmann::json j;
    j["user"] = "xiaoming";
    j["pwd"] = " daodoahdo  ";
    std::cout << j.dump(2, ' ', true) << '\n';
    std::string str = j.dump();
    coro_register<int>(ApiUserLogin(4,str,""),[](int ans){
        std::cout<<ans<<'\n';
    });

    }
    while(true);
}