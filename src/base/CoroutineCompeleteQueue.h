#ifndef COROUTINECOMPELETEQUEUE_H
#define COROUTINECOMPELETEQUEUE_H

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <chrono>
#include <unordered_set>
#include "RpcCoroutine.h"
#include "MPSCqueue.h"
#include "ThrdPool.h"

static constexpr int try_agian_times = 2; 

// 抽象基类：统一存储在队列里
struct NotifyBase
{
    virtual ~NotifyBase() = default;
    virtual bool try_notify() = 0; // ready? 调用回调并返回 true，否则 false
};
template<typename T>
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
        WorkPool::Instance().Submit([cb_ = std::move(cb) , v_ = task.get()]()
        { cb_(v_); });

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
        WorkPool::Instance().Submit([cb = std::move(cb)]()
        { cb(); });
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

    // 非 void 协程必须传 cb；
    // void 协程 cb 可选
    // 只支持移动语义

    template <typename T>
    void schedule(std::remove_reference_t< RpcTask<T>> &&t,
                  std::function<void(T)> cb )
    {
        auto nt = new Notify<T>(std::move(t), std::move(cb));
        auto h = nt->task.handle();
        h.promise().nt = nt;
        nt->task.resume();
     
    }
    void schedule(std::remove_reference_t<RpcTask<void>> &&t,
                  std::function<void()> cb = {})
    {
        if (!cb)
        {
            t.resume();
            return;
        }
        auto *nt = new Notify<void>(std::move(t), std::move(cb));
        auto h = nt->task.handle();
        h.promise().nt = nt;
        nt->task.resume();
    }
    template<typename T>
    void finish(Notify<T>*nt){
        pending_.Enqueue(nt);
    }

private:
    MPSCQueue<NotifyBase> pending_;
    std::chrono::milliseconds interval_{50};
    std::once_flag start_flag_;
    std::thread thread_handle;
    bool running;
    CoroutineScheduler():running(true)
    {
        std::call_once(start_flag_, [this]{
            this->thread_handle =std::thread(&CoroutineScheduler::run_loop,this);
        });
    }
    ~CoroutineScheduler(){
        running=false;
    }

    void run_loop()
    {
        while (running)
        {
            NotifyBase* nt;
            while(pending_.Dequeue(nt)){
                if(nt->try_notify()){
                delete nt;
                }else{
                   bool flag = false;
                   for(int i=0;i<try_agian_times;i++){
                        flag=nt->try_notify();
                        if(flag)break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                   }
                   if(!flag){
                    throw ;
                   }
                }
            }
            std::this_thread::sleep_for(interval_);
      
        }
    }
};

#endif 