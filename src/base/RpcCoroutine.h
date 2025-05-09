#ifndef COROTINUE_H
#define COROTINUE_H

#include <coroutine>
#include <optional>
#include <functional>
#include <iostream>
#include "GrpcClient.h"
#include <chrono>

using namespace rpc;

// 前向声明 Notify 和 Coroutine_finish
template <typename T>
struct Notify;

template <typename T>
void Coroutine_finish(Notify<T>* nt);

// RpcTask 模板及 void 特化

template<typename T>
class RpcTask {
public:
    struct promise_type {
        std::optional<T> result;
        Notify<T>* nt; // 通过 CoroutineScheduler 注入
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { 
            if (nt) {
                Coroutine_finish(nt);
            }
            return {};
         }

        RpcTask get_return_object() {
            return RpcTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        void return_value(T v) {
            result = std::move(v);
        }

        void unhandled_exception() {
            std::terminate();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) : coro_(h) {}
    ~RpcTask() {
        if (coro_) coro_.destroy();
    }

    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;

    // 移动语义
    RpcTask(RpcTask&& other) noexcept : coro_(other.coro_) {
        other.coro_ = nullptr;
    }
    RpcTask& operator=(RpcTask&& other) noexcept {
        if (this != &other) {
            if (coro_) coro_.destroy();
            coro_ = other.coro_;
            other.coro_ = nullptr;
        }
        return *this;
    }

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    T get() {
        while (!is_ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return std::move(*coro_.promise().result);
    }

    void resume() {
        if (coro_ && !coro_.done())
            coro_.resume();
    }

private:
    friend class CoroutineScheduler;
    handle_type& handle() noexcept { return coro_; }
    handle_type  coro_;
};

// void 专化

template<>
class RpcTask<void> {
public:
    struct promise_type {
        Notify<void>* nt;
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { 
            if (nt) {
                Coroutine_finish(nt);
            }
            return {};
         }

        RpcTask get_return_object() {
            return RpcTask{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        void return_void() noexcept {}
        void unhandled_exception() {
            std::terminate();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) : coro_(h) {}
    ~RpcTask() {
        if (coro_) coro_.destroy();
    }

    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;

    // 移动语义
    RpcTask(RpcTask&& other) noexcept : coro_(other.coro_) {
        other.coro_ = nullptr;
    }
    RpcTask& operator=(RpcTask&& other) noexcept {
        if (this != &other) {
            if (coro_) coro_.destroy();
            coro_ = other.coro_;
            other.coro_ = nullptr;
        }
        return *this;
    }

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    void resume() {
        if (coro_ && !coro_.done())
            coro_.resume();
    }

private:
    friend class CoroutineScheduler;
    handle_type& handle() noexcept { return coro_; }
    handle_type  coro_;
};

// 包含实现
#include "RpcCoroutine.hpp"

#endif // COROTINUE_H
