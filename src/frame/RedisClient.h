// RedisSingleton.h
#pragma once

#include <sw/redis++/redis++.h>
#include "Global.h"

inline sw::redis::Redis& get_redis() {
    static sw::redis::Redis redis = [](){
        // 从配置里读各项（带默认值）
        auto host     = Global::Instance().get<std::string>("Redis_Host");
        auto port     = Global::Instance().get<int>        ("Redis_Port");
        auto password = Global::Instance().get<std::string>("Redis_Password");
        auto poolSize = Global::Instance().get<size_t>     ("Redis_PoolSize");

        // 填充连接参数
        sw::redis::ConnectionOptions conn_opts;
        conn_opts.host = host;
        conn_opts.port = port;
        conn_opts.password = password;

        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = poolSize;

        return sw::redis::Redis{conn_opts, pool_opts};
    }();
    std::cout<<"redis init ok"<<'\n';
    return redis;
}
