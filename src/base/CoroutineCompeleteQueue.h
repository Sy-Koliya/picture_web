// CoroutineCompleteQueue.h
#ifndef COROUTINECOMPLETEQUEUE_H
#define COROUTINECOMPLETEQUEUE_H

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
#include "ThrdPool.h"
#include "RpcCoroutine.h"           
#include "MPSCqueue.h"           

static constexpr int try_again_times = 2;

struct NotifyBase {
    virtual ~NotifyBase() = default;
    virtual bool try_notify() = 0;
};

// T 版本 Notify
template <typename T>
struct Notify : std::enable_shared_from_this<Notify<T>>, NotifyBase {
    RpcTask<T>             task;
    std::function<void(T)> cb;

    Notify(RpcTask<T>&& t, std::function<void(T)> c)
      : task(std::move(t)), cb(std::move(c)) {}

    bool try_notify() override {
        if (!task.is_ready()) return false;
        if (cb) {
            auto self = this->shared_from_this();
            WorkPool::Instance().Submit([self]() {
                T value = self->task.get();
                self->cb(value);
            });
        }
        return true;
    }
};

// void 版本 Notify
template <>
struct Notify<void> : std::enable_shared_from_this<Notify<void>>, NotifyBase {
    RpcTask<void>    task;
    std::function<void()> cb;

    Notify(RpcTask<void>&& t, std::function<void()> c = {})
      : task(std::move(t)), cb(std::move(c)) {}

    bool try_notify() override {
        if (!task.is_ready()) return false;
        if (cb) {
            auto self = this->shared_from_this();
            WorkPool::Instance().Submit([self]() {
                self->cb();
            });
        }
        return true;
    }
};

class CoroutineScheduler {
public:
    static CoroutineScheduler& Instance() {
        static CoroutineScheduler inst;
        return inst;
    }

    template <typename T>
    void schedule(RpcTask<T>&& t, std::function<void(T)> cb = {}) {
        auto h = t.handle();
        void* key = h.address();
        auto nt = std::make_shared<Notify<T>>(std::move(t), std::move(cb));

        {
            std::lock_guard<std::mutex> lk(holders_mutex_);
            holders_.emplace(key, nt);
        }

        nt->task.resume();
    }

    // 在 promise.final_suspend() 中，由 handle.address() 直接调用
    void finish(void* key) {
        std::shared_ptr<NotifyBase> nt;
        {
            std::lock_guard<std::mutex> lk(holders_mutex_);
            auto it = holders_.find(key);
            if (it == holders_.end()) return;
            nt = it->second;
            holders_.erase(it);
        }
        // 推入 MPSC 队列，由队列管理生命周期
        pending_.Enqueue(std::move(nt));
        cv_.notify_one();
    }

private:
    CoroutineScheduler()
      : running_(true) {
        thread_handle_ = std::thread(&CoroutineScheduler::run_loop, this);
    }

    ~CoroutineScheduler() {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            running_.store(false, std::memory_order_release);
            cv_.notify_one();
        }
        if (thread_handle_.joinable())
            thread_handle_.join();
    }

    void run_loop() {
        std::unique_lock<std::mutex> lk(mutex_);
        while (running_) {
            cv_.wait(lk, [this] {
                return !pending_.empty() || !running_;
            });
            std::shared_ptr<NotifyBase> nb;
            while (pending_.Dequeue(nb)) {
                if (!nb->try_notify()) {
                    for (int i = 0; i < try_again_times; ++i) {
                        if (nb->try_notify()) break;
                        std::this_thread::sleep_for(
                          std::chrono::milliseconds(20));
                    }
                }
            }
        }
    }

    // key = coroutine frame 地址 (void*)
    std::mutex    holders_mutex_;
    std::unordered_map<void*, std::shared_ptr<NotifyBase>> holders_;

    // 队列存 shared_ptr<NotifyBase>
    MPSCQueueNonIntrusive<NotifyBase> pending_;
    std::mutex                  mutex_;
    std::condition_variable     cv_;
    std::thread                 thread_handle_;
    std::atomic<bool>           running_;
};

template <typename T>
inline void coro_register(RpcTask<T>&& task, std::function<void(T)> cb = {}) {
    CoroutineScheduler::Instance().schedule(std::move(task), std::move(cb));
}

#endif // COROUTINECOMPLETEQUEUE_H
