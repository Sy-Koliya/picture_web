// TimerEvent.h
#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include <functional>
#include <memory>
#include <cstdint>
#include <utility>
#include <unordered_set>
#include <mutex>
#include <iostream>
#include "tools.h"

// 前置声明

class TimerEvent
{
public:
    using Callback = std::function<void()>;

    TimerEvent(void *user_data,
               Callback callback,
               uint64_t interval,
               int te_id,
               int calltime = -1);

    ~TimerEvent() = default;

    // 执行回调
    void Execute();

    // 更新下次触发时间
    void UpdateNextTick();
    // 支持移动语义：转移 callback 所有权
    TimerEvent(const TimerEvent &) = delete;
    TimerEvent &operator=(const TimerEvent &) = delete;
    // 允许移动，转移 callback_ 等资源
    TimerEvent(TimerEvent &&) noexcept = default;
    TimerEvent &operator=(TimerEvent &&) noexcept = default;

public:
    void *user_data;    // 用户数据指针
    Callback callback;  // 动态分配回调函数，方便 Destroy
    uint64_t interval;  // 周期（毫秒）
    uint64_t next_tick; // 下次触发时刻
    int te_id;          // 唯一事件 ID
    int calltime;       // 剩余调用次数，-1 表示无限循环
};

class TimerEventManager : public NoCopy{
public:
    using callback_t = std::function<void()>;

    static TimerEventManager &Instance();

    int Create(callback_t &&callback,
                       uint64_t interval,
                       int calltime = -1,
                       void *user_data = nullptr);

    void Destroy(int handle);

private:
    TimerEventManager() = default;
    ~TimerEventManager() = default;
    mutable std::mutex m_lock;
    std::unordered_map<int, TimerEvent *> handle_;
    IDhelper idHelper_;
};

#endif // TIMER_EVENT_H