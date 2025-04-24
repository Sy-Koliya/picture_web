
#include "DatabaseEnvFwd.h"
#include "Log.h"

#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Implementation/SakilaDatabase.h"
#include "MySQLThreading.h"
#include "AsyncCallbackProcessor.h"
#include "QueryHolder.h"
#include <chrono>
#include <memory>
#include <thread>

int main()
{
    MySQL::Library_Init();

    DatabaseLoader loader;
    loader.AddDatabase(SakilaDatabase, "127.0.0.1;3306;root;js2004521;tuchuang",
                       8, 2);

    if (!loader.Load())
    {
        TC_LOG_ERROR("", "TuchuangDatabase connect error");
        return 1;
    }
    TC_LOG_INFO("", "TuchuangDatabase connect success");
    { // 同步
        // SakilaDatabase.DirectExecute("insert into actor(first_name, last_name) values ('mark', '0voice')");
        auto result = SakilaDatabase.Query("select id, nick_name, user_name from user_info where id  = 15");
        if (!result)
        {
            TC_LOG_ERROR("", "select empty");
            return 1;
        }
        TC_LOG_INFO("", "id=%ld,nick_name=%s,user_name=%s",
                    (*result)[0].GetInt64(), (*result)[1].GetString(), (*result)[2].GetString());
    }
    SakilaDatabase.Close();
    MySQL::Library_End();
    return 0;
}