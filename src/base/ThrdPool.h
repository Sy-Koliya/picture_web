#ifndef THRDPOOL_H
#define THRDPOOL_H

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <thread>
#include "BaseThrdPool.h"
#include "tools.h"
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

private:
    static void ExecuteTask(void* context);
    static size_t thrdpool_size( thrdpool_t* pool);

    thrdpool_t* pool_;
};

#endif