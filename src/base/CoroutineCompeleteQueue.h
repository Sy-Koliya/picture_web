#ifndef COROUTINECOMPLETEQUEUE_H
#define COROUTINECOMPLETEQUEUE_H

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ThrdPool.h"
#include "RpcCoroutine.h"
#include "MPSCqueue.h"

static constexpr int try_again_times = 2;

// 基类添加 enable_shared_from_this 支持
struct NotifyBase {
    virtual ~NotifyBase() = default;
    virtual bool try_notify() = 0;
};

template <typename T>
class RpcTask;
// T 版本
template <typename T>
struct Notify : NotifyBase,public std::enable_shared_from_this<Notify<T>> {
    RpcTask<T> task;
    std::function<void(T)> cb;

    Notify(RpcTask<T>&& t, std::function<void(T)> cb)
        : task(std::move(t)), cb(std::move(cb)) {}

    bool try_notify() override {
        if (!task.is_ready()) return false;
        if (cb) {
            auto self = this->shared_from_this();
            WorkPool::Instance().Submit([self] {
                T value = self->task.get(); // 安全访问
                self->cb(value);
            });
        }
        return true;
    }
};

// void 版本
template <>
struct Notify<void> : NotifyBase,public std::enable_shared_from_this<Notify<void>>
{
    RpcTask<void> task;
    std::function<void()> cb;

    Notify(RpcTask<void> &&t, std::function<void()> c = {})
        : task(std::move(t)), cb(std::move(c)) {}

    bool try_notify() override
    {
        if (!task.is_ready())
            return false;
        if (cb)
        {
            auto self = this->shared_from_this();
            WorkPool::Instance().Submit([self](){
                                       self->cb(); });
        }
        return true;
    }
};

class CoroutineScheduler
{
public:
    static CoroutineScheduler &Instance()
    {
        static CoroutineScheduler inst;
        return inst;
    }
    template <typename T>
    void schedule(RpcTask<T>&& t, std::function<void(T)> cb = {})
    {
        auto nt = std::make_shared<Notify<T>>(std::move(t), std::move(cb));
        auto h = nt->task.handle();
        h.promise().nt = nt;
        nt->task.resume();
    }

    template <typename T>
    void finish(std::shared_ptr<Notify<T>> nt) {
        pending_.Enqueue(std::static_pointer_cast<NotifyBase>(nt));
        {
            std::lock_guard<std::mutex> lk(mutex_);
            cv_.notify_one();
        }
    }

private:
    CoroutineScheduler()
        : running_(true)
    {
        thread_handle_ = std::thread(&CoroutineScheduler::run_loop, this);
    }

    ~CoroutineScheduler()
    {
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
            cv_.wait(lk, [this] { return !pending_.empty() || !running_; });

            std::shared_ptr<NotifyBase> nb;
            while (pending_.Dequeue(nb)) {
                if (nb->try_notify()) {
                    // 无需手动释放，shared_ptr 自动管理
                } else {
                    for (int i = 0; i < try_again_times; ++i) {
                        if (nb->try_notify()) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                }
            }
        }
    }

    MPSCQueueNonIntrusive<NotifyBase> pending_; 
    std::mutex                  mutex_;
    std::condition_variable     cv_;
    std::thread                 thread_handle_;
    std::atomic<bool>           running_;
};

// 帮助注册函数
template <typename T>
inline void coro_register(RpcTask<T>&& task, std::function<void(T)> cb = {})
{
    CoroutineScheduler::Instance().schedule(std::move(task), std::move(cb));
}

#endif // COROUTINECOMPLETEQUEUE_H
