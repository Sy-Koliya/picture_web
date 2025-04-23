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
#include <iostream>

//===============================================================================================================


static size_t getDeviceMaxThreads() {
    unsigned int hc = std::thread::hardware_concurrency();
    return (hc > 0 ? static_cast<size_t>(hc) : 1);
}

void ThreadPool::CreatPool(size_t nthreads,size_t stacksize){
    pool_ =  thrdpool_create(nthreads, stacksize);
    if (!pool_)
    {
        throw std::runtime_error("Failed to create thread pool");
    }
}

ThreadPool::ThreadPool(size_t nthreads, size_t stacksize )
{
    CreatPool(nthreads,stacksize);
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



//=====================================================================================================


SocketPool& SocketPool::Instance(size_t nthreads ){
        nthreads=std::min(nthreads,getDeviceMaxThreads());
        size_t nthds=align_pow_2(nthreads);
        if(nthds==0){
            throw std::runtime_error("Failed to create thread, Max useful thread is zero ");
        };
        static SocketPool socket_pool_t(nthds);
        return socket_pool_t;
}


SocketPool::SocketPool(size_t nthreads)
:nthreads(nthreads),ThreadPool(nthreads)
{
    for(size_t i = 0 ;i<nthreads;++i){
        EventDispatch* ev_dispatch = new EventDispatch{};
        Submit(&EventDispatch::StartDispatch,*ev_dispatch,0);//非阻塞
        ev_queue.push_back(ev_dispatch);
    }
}
SocketPool::~SocketPool(){
    for(int i=0;i<nthreads;i++){
        ev_queue[i]->StopDispatch();
    }

}

void SocketPool::AddSocketEvent(int fd,uint32_t event){
    std::cout
    << "AddEvent event " << event
    << "  fd " << fd
    << '\n';
    int target = fd&(nthreads-1);
    FindBaseSocket(fd)->SetEventDispatch(ev_queue[target]);
    ev_queue[target]->AddEvent(fd,event);
}

void SocketPool::AddTimerEvent(TimerEvent* ev){
    int fd =ev->te_id;
    int target = fd&(nthreads-1);
    ev_queue[target]->AddTimer(ev);
}

//=========================================================================
WorkPool::WorkPool(size_t nthreads ):ThreadPool(nthreads){} 