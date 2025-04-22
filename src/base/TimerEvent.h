// TimerEvent.h
#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include <functional>
#include <memory>
#include <cstdint>
#include <utility>
#include <unordered_set>
#include <mutex>


// 前置声明

class TimerEvent
{
public:
    using Callback = std::function<void()>;

    // 构造函数：传入用户数据、回调函数指针、间隔、事件ID和调用次数
    TimerEvent(void *user_data,
               Callback callback,
               uint64_t interval,
               int te_id,
               int calltime = -1)
        : user_data(user_data),
          callback(callback),
          interval(interval),
          next_tick(get_tick_count()),
          te_id(te_id),
          calltime(calltime)
    {
    }

    // 析构时自动销毁回调指针
    ~TimerEvent(){}

    // 执行回调
    void Execute()
    {
        callback();
    }

    // 更新下次触发时间
    void UpdateNextTick()
    {
        next_tick += interval;
    }
    // 支持移动语义：转移 callback 所有权
    TimerEvent(const TimerEvent&) = delete;
    TimerEvent& operator=(const TimerEvent&) = delete;
    // 允许移动，转移 callback_ 等资源
    TimerEvent(TimerEvent&&) noexcept = default;
    TimerEvent& operator=(TimerEvent&&) noexcept = default;

public:
    void *user_data;    // 用户数据指针
    Callback callback; // 动态分配回调函数，方便 Destroy
    uint64_t interval;  // 周期（毫秒）
    uint64_t next_tick; // 下次触发时刻
    int te_id;          // 唯一事件 ID
    int calltime;       // 剩余调用次数，-1 表示无限循环
};


class TimerEventManager : public NoCopy
{
public:
    using callback_t = std::function<void()>;

    static TimerEventManager &Instance()
    {
        static TimerEventManager inst;
        return inst;
    }
    
    TimerEvent* createEvent(callback_t &callback,
                           uint64_t interval,
                           int calltime = -1,
                           void* user_data = nullptr)
    {
        std::lock_guard<std::mutex> guard (m_lock);
        TimerEvent * te = new TimerEvent(user_data, std::move(callback), interval, idHelper_.Get(), calltime);
        handle_.insert(std::make_pair(te->te_id,te));
        return te;
    }
    void destoryEvent(TimerEvent* te){
        std::lock_guard<std::mutex> guard (m_lock);//注意，此处并不能保证其他线程对于te指针的销毁，管理仍需谨慎！
        if(te==nullptr)return ;
        int id= te->te_id;
        if(handle_.find(id)!=handle_.end()){
            idHelper_.Del(te->te_id);
            handle_.erase(id);
            delete te;
           
        }
    }
    void destoryEvent(int handle){
        std::lock_guard<std::mutex> guard (m_lock);
        if(handle_.find(handle)!=handle_.end()){
            idHelper_.Del(handle);
            TimerEvent* te = handle_[handle];
            if(te!=nullptr)delete te;
            handle_.erase(handle);
        }
    }
private:
    TimerEventManager() = default;
    ~TimerEventManager(){
        for(auto hd:handle_){
            TimerEvent* ptr = hd.second;
            if(ptr!=nullptr){
                delete ptr;
            }
        }
    }
    mutable std::mutex m_lock;
    std::unordered_map<int ,TimerEvent*>handle_; 
    IDhelper idHelper_;
};

#endif // TIMER_EVENT_H