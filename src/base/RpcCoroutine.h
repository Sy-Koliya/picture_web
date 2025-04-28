#ifndef COROTINUE_H
#define COROTINUE_H

#include <coroutine>
#include <optional>
#include <functional>
#include <iostream>
#include "GrpcClient.h"     

using namespace rpc;


// T ≠ void 版本
template<typename T>
class RpcTask {
public:
    struct promise_type {
        std::optional<T> result;

        std::suspend_never initial_suspend() noexcept { return {}; }
        // 协程结束自动清理
        std::suspend_never   final_suspend()   noexcept { return {}; }

        RpcTask get_return_object() {
            return RpcTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        // 非 void 协程必须提供 return_value
        void return_value(T v) {
            result = std::move(v);
        }

        void unhandled_exception() {
            std::terminate();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) : coro_(h) {}
    //rpc的协程调用是多次且在线程池中的，不需要通过句柄对协程帧进行管理
    ~RpcTask() = default; 

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    // 只有非 void 才有返回值
    T get() {
        return std::move(*coro_.promise().result);
    }

    // 手动触发一次 resume（如果需要的话）
    void resume() {
        if (coro_ && !coro_.done()) coro_.resume();
    }

private:
    handle_type coro_;
};

// T = void 版本
template<>
class RpcTask<void> {
public:
    struct promise_type {
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never   final_suspend()   noexcept { return {}; }
        RpcTask get_return_object() {
            return RpcTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        // void 协程必须提供 return_void
        void return_void() noexcept {}

        void unhandled_exception() {
            std::terminate();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) : coro_(h) {}
    ~RpcTask() =default;

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    // void 版本的 get() 只是保证类型存在，什么都不返回
    void get() {}

    void resume() {
        if (coro_ && !coro_.done()) coro_.resume();
    }

private:
    handle_type coro_;
};

#define MYSQL_RPC_CALL(Method)                                                         \
inline template<typename Req, typename Resp>                                           \
auto Mysql##Method##Call( MysqlClient<Req,Resp>* client, Req req )                     \
    -> RpcAwaitable<Req, Resp, &DatabaseService::Stub::PrepareAsync##Method>           \
{                                                                                      \
    return { client, std::move(req) };                                                 \
}

// 用户写 co_await MySqlRegisterCall(...)

template<typename Req, typename Resp>
inline auto MysqlRegisterCall(MysqlClient<Req,Resp>* client, Req req) {
    return RpcAwaitable<Req,Resp,&DatabaseService::Stub::PrepareAsyncregisterUser>{client, std::move(req)};
}


#endif