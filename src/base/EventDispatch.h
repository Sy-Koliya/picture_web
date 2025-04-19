/*
 * A socket event dispatcher, features include:
 * 1. portable: worked both on Windows, MAC OS X,  LINUX platform
 * 2. a singleton pattern: only one instance of this class can exist
 */
#ifndef __EVENT_DISPATCH_H__
#define __EVENT_DISPATCH_H__

#include "types.h"
#include "tools.h"
#include <mutex>
#include <unordered_map>
#include <queue>
#include <vector>

struct TimerEvent
{
    void* user_data;
    callback_t callback;
    uint64_t interval;
    uint64_t next_tick;
    int te_id;
    int calltime;   //-1一直循环
};

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

class CEventDispatch
{
public:
	virtual ~CEventDispatch();

	void AddEvent(int  fd, uint8_t socket_event);
	void RemoveEvent(int fd, uint8_t socket_event);

    int AddTimer(TimerEvent * te);
	int AddTimer(callback_t callback, void *user_data, uint64_t interval,int calltime);
	void RemoveTimer(int handlie_te_id);

	void AddLoop(callback_t callback, void *user_data);

	void StartDispatch(uint32_t wait_timeout = 100);
	void StopDispatch();

	bool isRunning() { return running; }

	static CEventDispatch &Instance();

protected:
	CEventDispatch();

private:
	void _CheckTimer();
	void _CheckLoop();

private:

	int m_epfd;
	mutable std::mutex m_lock;
	std::vector<TimerEvent *> m_loop_list;
	static CEventDispatch *m_pEventDispatch;
    std::priority_queue<
        TimerEvent*,
        std::vector<TimerEvent*>,
        TimerMinCmp
    > m_timer_list;
    std::unordered_map<int,TimerEvent*>handle_te;
	bool running;
    IDhelper id_helper;
};

#endif
