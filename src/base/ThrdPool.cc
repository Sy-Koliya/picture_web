/*
 * ThreadPool.h
 *
 * C++ wrapper around the Sogou C Workflow thrdpool API.
 * Provides a simple RAII-style thread pool base class, allowing
 * enqueueing of any callable returning void().
 *
 * Usage:
 *   // Derive or use directly
 *   ThreadPool pool(4);     // 4 worker threads
 *   pool.enqueue([]{ do_work(); });
 *   // tasks run asynchronously, destructor waits for termination
 *
 * Author: Adapted from Sogou Workflow C thread pool
 */

extern "C"
{
#include "msgqueue.h"
#include "BaseThrdPool.h"
}
#include "ThrdPool.h"
#include "tools.h"

ThreadPool::ThreadPool(size_t nthreads, size_t stacksize )
{
    pool_ =  thrdpool_create(nthreads, stacksize);
    if (!pool_)
    {
        throw std::runtime_error("Failed to create thread pool");
    }
}

ThreadPool::~ThreadPool()
{
    thrdpool_destroy([](const thrdpool_task *task)
                     {
                auto func_ptr = static_cast<std::function<void()>*>(task->context);
                delete func_ptr; }, pool_);
}


size_t ThreadPool::ThreadCount() const
{
    return thrdpool_size(pool_);
}


void ThreadPool::ExecuteTask(void *context)
{
    std::unique_ptr<std::function<void()>> func(
        static_cast<std::function<void()> *>(context));
    (*func)();
}

size_t ThreadPool::thrdpool_size(thrdpool_t *pool)
{
    return pool->nthreads;
}

thrdpool_t *pool_;
