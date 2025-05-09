
#include "TimerEvent.h"
#include "ThrdPool.h"
#include <memory>


TimerEvent::TimerEvent(void *user_data,
                       Callback callback,
                       uint64_t interval,
                       int te_id,
                       int calltime)
    : user_data(user_data),
      callback(callback),
      interval(interval),
      next_tick(get_tick_count() + interval),
      te_id(te_id),
      calltime(calltime)
{}
void TimerEvent::Execute()
{
    callback();
}

void TimerEvent::UpdateNextTick()
{
    next_tick += interval;
}

TimerEventManager& TimerEventManager::Instance()
{
    static TimerEventManager inst;
    return inst;
}

int TimerEventManager::Create(callback_t &&callback,
                                      uint64_t interval,
                                      int calltime ,
                                      void *user_data )
{
    std::lock_guard<std::mutex> guard(m_lock);
    TimerEvent* te_ptr = new TimerEvent( user_data, std::move(callback), interval, idHelper_.Get(), calltime);
    int handle = te_ptr->te_id;
    handle_[handle]=te_ptr;
    SocketPool::Instance().AddTimerEvent(te_ptr);
    return handle;
}


void TimerEventManager::Destroy(int handle)
{
    std::lock_guard<std::mutex> guard(m_lock);
    if (handle_.find(handle) != handle_.end())
    {
        idHelper_.Del(handle);
        TimerEvent *te = handle_[handle];
        SocketPool::Instance().RemoveTimerEvent(te);
        handle_.erase(handle);
    }
    else
    {
        std::cout << "No tracked TimerEvent  found!" << std::endl;
    }
}
