/*
 * A socket event dispatcher, features include:
 */
#ifndef __EVENT_DISPATCH_H__
#define __EVENT_DISPATCH_H__

#include "types.h"
#include "tools.h"
#include "TimerEvent.h"
#include <mutex>
#include <unordered_map>
#include <queue>
#include <vector>

struct TimerMinCmp {
    bool operator()(TimerEvent* a, TimerEvent* b) const {
        // 都为空，认为 a 不“落后”于 b
        if (!a && !b) return false;
        // a 为空，b 不为空 → a 放到后面（返回 true）
        if (!a) return true;
        // b 为空，a 不为空 → a 放到前面（返回 false）
        if (!b) return false;
        // 普通情况，next_tick 大的“落后”于 next_tick 小的
        return a->next_tick > b->next_tick;
    }
};

class EventDispatch
{

using callback_t = std::function<void()>;

public:
    EventDispatch();
	virtual ~EventDispatch();

	void AddEvent(int  fd, uint8_t socket_event);
	void RemoveEvent(int fd, uint8_t socket_event);

    void AddTimer(TimerEvent * te);
	void RemoveTimer(int handlie_te_id);

	void AddLoop(TimerEvent * te);

	void StartDispatch(uint32_t wait_timeout = 100);
	void StopDispatch();

	bool isRunning() { return running; }


private:
	void _CheckTimer();
	void _CheckLoop();

private:

	int m_epfd;
	mutable std::mutex m_lock;
	std::vector<TimerEvent *> m_loop_list;
    std::priority_queue<
        TimerEvent*,
        std::vector<TimerEvent*>,
        TimerMinCmp
    > m_timer_list;
    std::unordered_map<int,TimerEvent*>handle_te;
	bool running;
};

#endif
