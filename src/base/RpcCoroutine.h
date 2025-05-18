// RpcCoroutine.h
#ifndef COROTINUE_H
#define COROTINUE_H

#include <coroutine>
#include <optional>
#include <functional>
#include <atomic>
#include "GrpcClient.h"          // 需要能看到 GrpcClient 的声明

void coro_finish(void *ptr);

template <typename T>
class RpcTask {
public:
    struct promise_type {
        std::optional<T>     result;
        std::atomic<bool>    enqueued_{false};

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            // 只第一次触发入队
            if (!enqueued_.exchange(true)) {
               coro_finish(h.address());
            }
            return {};
        }

        RpcTask get_return_object() {
            return RpcTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        void return_value(T v) {
            result = std::move(v);
        }

        void unhandled_exception() {
            std::terminate();
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) noexcept : coro_(h) {}
    ~RpcTask() {
        if (coro_ && coro_.done()) coro_.destroy();
    }

    RpcTask(RpcTask&& o) noexcept : coro_(o.coro_) { o.coro_ = nullptr; }
    RpcTask& operator=(RpcTask&& o) noexcept {
        if (this != &o) {
            if (coro_ && coro_.done()) coro_.destroy();
            coro_ = o.coro_;
            o.coro_ = nullptr;
        }
        return *this;
    }
    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;

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

    handle_type handle() noexcept { return coro_; }

private:
    handle_type coro_;
};

// void 特化
template <>
class RpcTask<void> {
public:
    struct promise_type {
        std::atomic<bool> enqueued_{false};

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept {
            auto h = std::coroutine_handle<promise_type>::from_promise(*this);
            if (!enqueued_.exchange(true)) {
              coro_finish(h.address());
            }
            return {};
        }

        RpcTask get_return_object() {
            return RpcTask{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit RpcTask(handle_type h) noexcept : coro_(h) {}
    ~RpcTask() {
        if (coro_ && coro_.done()) coro_.destroy();
    }

    RpcTask(RpcTask&& o) noexcept : coro_(o.coro_) { o.coro_ = nullptr; }
    RpcTask& operator=(RpcTask&& o) noexcept {
        if (this != &o) {
            if (coro_ && coro_.done()) coro_.destroy();
            coro_ = o.coro_;
            o.coro_ = nullptr;
        }
        return *this;
    }
    RpcTask(const RpcTask&) = delete;
    RpcTask& operator=(const RpcTask&) = delete;

    bool is_ready() const noexcept {
        return coro_ && coro_.done();
    }

    void resume() {
        if (coro_ && !coro_.done())
            coro_.resume();
    }

    handle_type handle() noexcept { return coro_; }

private:
    handle_type coro_;
};

#endif // COROTINUE_H
