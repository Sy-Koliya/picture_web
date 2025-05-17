#ifndef COROTINUE_H
#define COROTINUE_H

#include <coroutine>
#include <optional>
#include <functional>
#include <iostream>
#include <atomic>
#include "GrpcClient.h"
#include <chrono>

using namespace rpc;

// 前向声明 Notify 和 Coroutine_finish
template <typename T>
struct Notify;

template <typename T>
void Coroutine_finish(Notify<T> *nt);

// RpcTask 模板及 void 特化

template <typename T>
class RpcTask {
public:
    struct promise_type {
        std::optional<T> result;
        Notify<T> *nt = nullptr; // 通过 CoroutineScheduler 注入
        std::atomic<bool> enqueued_{false};

        std::suspend_always initial_suspend() noexcept { return {}; }
        
        std::suspend_always final_suspend() noexcept {
            bool expected = false;
            if (nt && enqueued_.compare_exchange_strong(expected, true)) { // 原子操作
                Coroutine_finish(nt);
            }
            return {};
        }

        RpcTask get_return_object() {
            return RpcTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_value(T v) {
            result = std::move(v);
        }

        void unhandled_exception() {
            std::terminate();
        }

    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) noexcept : coro_(h), destroyed_(false) {}

    ~RpcTask() {
        if (coro_ && !destroyed_) {
             if (coro_.done()) coro_.destroy();
             destroyed_ = true;
        }
    }

    RpcTask(const RpcTask &) = delete;
    RpcTask &operator=(const RpcTask &) = delete;

    RpcTask(RpcTask &&other) noexcept 
        : coro_(other.coro_), destroyed_(other.destroyed_) {
        other.coro_ = nullptr;
        other.destroyed_ = true;
    }

    RpcTask &operator=(RpcTask &&other) noexcept {
        if (this != &other) {
            if (coro_ && !destroyed_) coro_.destroy();
            coro_ = other.coro_;
            destroyed_ = other.destroyed_;
            other.coro_ = nullptr;
            other.destroyed_ = true;
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
        if (coro_ && !coro_.done()) coro_.resume();
    }

private:
    friend class CoroutineScheduler;
    handle_type &handle() noexcept { return coro_; }
    handle_type coro_;
    bool destroyed_;
};

// void 特化
template <>
class RpcTask<void> {
public:
    struct promise_type {
        Notify<void> *nt = nullptr;
        std::atomic<bool> enqueued_{false};

        std::suspend_always initial_suspend() noexcept { return {}; }
        
        std::suspend_always final_suspend() noexcept {
            bool expected = false;
            if (nt && enqueued_.compare_exchange_strong(expected, true)) {
                Coroutine_finish(nt);
            }
            return {};
        }

        RpcTask get_return_object() {
            return RpcTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        void return_void() noexcept {}

        void unhandled_exception() {
            std::terminate();
        }

    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) noexcept : coro_(h), destroyed_(false) {}

    RpcTask(const RpcTask &) = delete;
    RpcTask &operator=(const RpcTask &) = delete;

    RpcTask(RpcTask &&other) noexcept 
        : coro_(other.coro_), destroyed_(other.destroyed_) {
        other.coro_ = nullptr;
        other.destroyed_ = true;
    }

    RpcTask &operator=(RpcTask &&other) noexcept {
        if (this != &other) {
            if (coro_ && !destroyed_) coro_.destroy();
            coro_ = other.coro_;
            destroyed_ = other.destroyed_;
            other.coro_ = nullptr;
            other.destroyed_ = true;
        }
        return *this;
    }

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    void resume() {
        if (coro_ && !coro_.done()) coro_.resume();
    }

private:
    friend class CoroutineScheduler;
    handle_type &handle() noexcept { return coro_; }
    handle_type coro_;
    bool destroyed_;
};

#include "RpcCoroutine.hpp"

#endif // COROTINUE_H