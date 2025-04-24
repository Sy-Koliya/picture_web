#include "EventDispatch.h"
#include "BaseSocket.h"
#include "ThrdPool.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

#define MIN_TIMER_DURATION 100

EventDispatch::EventDispatch()
{
    running = false;
    m_epfd = epoll_create(1024);
    if (m_epfd == -1)
    {
        std::cerr
            << "epoll_create failed"
            << "\n";
    }
}

EventDispatch::~EventDispatch()
{
    StopDispatch();
}

void EventDispatch::AddTimer(TimerEvent *te)
{
    handle_te[te->te_id] = te;
    m_timer_list.push(te);
}

void EventDispatch::RemoveTimer(int handle_te_id)
{
    if (handle_te.find(handle_te_id) == handle_te.end())
        return;
    TimerEvent *te = handle_te[handle_te_id];
    handle_te.erase(handle_te_id);
}

void EventDispatch::_CheckTimer()
{
    uint64_t curr_tick = get_tick_count();
    while (!m_timer_list.empty())
    {
        TimerEvent *te = m_timer_list.top();
        if (te == nullptr)
        {
            m_timer_list.pop();
            continue;
        }
        if (te->next_tick <= curr_tick)
        {
            m_timer_list.pop();
            WorkPool::Instance().Submit(te->callback);
            if (te->calltime == -1 || --te->calltime != 0)
            {
                te->next_tick = curr_tick + te->interval;
                m_timer_list.push(te);
            }
            else
            {
                RemoveTimer(te->te_id);
            }
        }
        else
        {
            break;
        }
    }
}

void EventDispatch::AddLoop(TimerEvent *te)
{
    m_loop_list.push_back(te);
}

void EventDispatch::_CheckLoop()
{
    for (auto it : m_loop_list)
    {
        (it->callback)();
    }
}

void EventDispatch::AddEvent(int fd, uint32_t socket_event)
{
    struct epoll_event ev;
    ev.events = socket_event;
    ev.data.fd = fd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        std::cerr
            << "epoll_ctl() failed, errno=" << errno
            << "\n";
    }
}

void EventDispatch::RemoveEvent(int fd)
{
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) != 0)
    {
        std::cerr
            << "epoll_ctl failed, errno=" << errno
            << "\n";
    }
}

void EventDispatch::StartDispatch(uint32_t wait_timeout)
{
    struct epoll_event events[1024];
    int nfds = 0;

    if (running)
        return;
    running = true;

    while (running)
    {
        nfds = epoll_wait(m_epfd, events, 1024, wait_timeout);
        for (int i = 0; i < nfds; i++)
        {
            int ev_fd = events[i].data.fd;
            BaseSocket *pSocket = FindBaseSocket(ev_fd);
            if (!pSocket)
                continue;

// Commit by zhfu @2015-02-28
#ifdef EPOLLRDHUP
            if (events[i].events & EPOLLRDHUP)
            {
                pSocket->OnClose();
            }
#endif
            // Commit End

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                pSocket->OnClose();
            }
            if (events[i].events & EPOLLIN)
            {
                pSocket->OnRead();
            }
            if (events[i].events & EPOLLOUT)
            {
                pSocket->OnWrite();
            }
        }

        _CheckTimer();
        _CheckLoop();
    }
}

void EventDispatch::StopDispatch()
{
    // 能否平滑退出
    running = false;
    close(m_epfd);
}
