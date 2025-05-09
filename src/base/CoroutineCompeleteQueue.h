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

struct NotifyBase
{
    virtual ~NotifyBase() = default;
    virtual bool try_notify() = 0; // ready? 调用回调并返回 true，否则 false
};

template <typename T>
class RpcTask;

// T 版本
template <typename T>
struct Notify : NotifyBase
{
    RpcTask<T> task;
    std::function<void(T)> cb;

    Notify(RpcTask<T> &&t, std::function<void(T)> cb)
        : task(std::move(t)), cb(std::move(cb)) {}

    bool try_notify() override
    {
        if (!task.is_ready())
            return false;
        if (cb)
        {
            WorkPool::Instance().Submit([cb_ = std::move(cb), v_ = task.get()]()
                                        { cb_(v_); });
        }
        return true;
    }
};

// void 版本
template <>
struct Notify<void> : NotifyBase
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
            WorkPool::Instance().Submit([cb = std::move(cb)]()
                                        { cb(); });
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
        auto* nt = new Notify<T>(std::move(t), std::move(cb));
        auto h = nt->task.handle();
        h.promise().nt = nt;
        nt->task.resume();
    }

    template <typename T>
    void finish(Notify<T>* nt)
    {
        pending_.Enqueue(nt);
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

    void run_loop()
    {
        std::unique_lock<std::mutex> lk(mutex_);
        while (running_.load(std::memory_order_acquire))
        {
            // 等待 new tasks or stop signal
            cv_.wait(lk, [this] { return !pending_.empty() || !running_.load(); });

            // 调度完成队列
            NotifyBase* nb = nullptr;
            while (pending_.Dequeue(nb))
            {
                if (nb->try_notify())
                {
                    delete nb;
                }
                else
                {
                    bool ok = false;
                    for (int i = 0; i < try_again_times; ++i)
                    {
                        if (nb->try_notify())
                        {
                            ok = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    if (!ok)
                        throw std::runtime_error("CoroutineScheduler: notify failed");
                    delete nb;
                }
            }
        }
    }

    MPSCQueue<NotifyBase>      pending_;
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
