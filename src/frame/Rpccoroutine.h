#ifndef COROTINUE_H
#define COROTINUE_H

#include <coroutine>
#include <optional>
#include <functional>
#include <iostream>
#include "GrpcClient.h"
#include "ThrdPool.h"           // 线程池接口
#include "TimerEvent.h"  // 定时器管理器接口
#include "EventDispatch.h"

using namespace rpc;



#include <coroutine>
#include <optional>
#include <type_traits>




template<typename T = void>
class RpcTask {
public:
    struct promise_type {
        // 仅当 T 非 void 时保留 result
        std::conditional_t<!std::is_void_v<T>, std::optional<T>, std::monostate> result;

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        RpcTask get_return_object() {
            return RpcTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        // 处理非 void 类型的返回
        template<typename U = T>
        requires (!std::is_void_v<U>)
        void return_value(U&& v) {
            result = std::forward<U>(v);
        }

        // 处理 void 类型的返回
        void return_void() requires (std::is_void_v<T>) {}

        void unhandled_exception() { std::terminate(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) : coro_(h) {}
    ~RpcTask() { if (coro_) coro_.destroy(); }

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    // 仅当 T 非 void 时启用 get()
    auto get() requires (!std::is_void_v<T>) {
        return std::move(*coro_.promise().result);
    }

    // 当 T 是 void 时专用版本
    void get() requires (std::is_void_v<T>) {
        coro_.promise().result; // 无操作，保持语法有效性
    }

    void resume() {
        if (coro_ && !coro_.done())
            coro_.resume();
    }

private:
    handle_type coro_;
};


// 用户写 co_await AsyncMySqlCall(...)

template<typename Req, typename Resp>
auto MysqlRegisterCall(MysqlClient<Req,Resp>* client, Req req) {
    return RpcAwaitable<Req,Resp,&DatabaseService::Stub::PrepareAsyncregisterUser>{client, std::move(req)};
}

 // RegisterUser
// auto reg_resp = co_await AsyncRpc<
//     RegisterRequest, RegisterResponse,
//     &DatabaseService::Stub::PrepareAsyncregisterUser
// >(mysql_client.get(), std::move(reg_req));

// Login
// auto login_resp = co_await AsyncRpc<
//     LoginRequest, LoginResponse,
//     &DatabaseService::Stub::PrepareAsynclogin
// >(mysql_client.get(), std::move(login_req));


#endif