#ifndef THRDPOOL_H
#define THRDPOOL_H

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <thread>
#include <vector>
#include "BaseThrdPool.h"
#include "tools.h"
#include "BaseSocket.h"
#include "EventDispatch.h"
#include "TimerEvent.h"

struct thrdpool_task;

class ThreadPool:public NoCopy {
public:
    explicit ThreadPool(size_t nthreads, size_t stacksize = 0);
    ~ThreadPool();

    template <typename Func, typename... Args>
    void Submit(Func &&f, Args &&...args)
    {
        auto task_func = Package2FVV(std::forward<Func>(f), std::forward<Args>(args)...);
        auto func_ptr = new std::function<void()>(std::move(task_func));
    
        thrdpool_task task;
        task.routine = &ThreadPool::ExecuteTask;
        task.context = func_ptr;
    
        if (thrdpool_schedule(&task, pool_) != 0)
        {
            delete func_ptr;
            throw std::runtime_error("Failed to submit task");
        }
    }
    size_t ThreadCount() const;

protected:
    void CreatPool(size_t nthreads,size_t stacksize);
    static void ExecuteTask(void* context);
    static size_t thrdpool_size( thrdpool_t* pool);

    thrdpool_t* pool_;
};


//所有事件加入通过该池,监听事件和计时事件
class SocketPool:public ThreadPool{

public: 
    static SocketPool& Instance(size_t nthreads = 4);
    void  AddSocketEvent(int fd,uint8_t event); //可以根据 fd拿到BaseSocket
    void  AddTimerEvent(TimerEvent* ev);

private :
    explicit SocketPool(size_t nthreads);
    ~SocketPool();

private:
    size_t nthreads;
    std::vector<EventDispatch*>ev_queue;
};
















#endif