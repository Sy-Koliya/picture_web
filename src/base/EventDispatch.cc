#include "EventDispatch.h"
#include "BaseSocket.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

#define MIN_TIMER_DURATION 100 // 100 miliseconds

EventDispatch::EventDispatch()
{
    running = false;
    m_epfd = epoll_create(1024);
    if (m_epfd == -1)
    {
        printf("epoll_create failed");
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
            te->Execute(); //此处后面加入线程池
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
        else{
            break;
        }
    }
}

void EventDispatch::AddLoop(TimerEvent* te)
{
    m_loop_list.push_back(te);
}

void EventDispatch::_CheckLoop()
{
    for(auto it:m_loop_list){
        (it->callback)();
    }
}



void EventDispatch::AddEvent(int fd, uint8_t socket_event)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLPRI | EPOLLERR | EPOLLHUP;
    ev.data.fd = fd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        printf("epoll_ctl() failed, errno=%d", errno);
    }
}

void EventDispatch::RemoveEvent(int fd, uint8_t socket_event)
{
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) != 0)
    {
        printf("epoll_ctl failed, errno=%d", errno);
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
                // printf("On Peer Close, socket=%d, ev_fd);
                pSocket->OnClose();
            }
#endif
            // Commit End

            if (events[i].events & EPOLLIN)
            {
                // printf("OnRead, socket=%d\n", ev_fd);
                pSocket->OnRead();
            }

            if (events[i].events & EPOLLOUT)
            {
                // printf("OnWrite, socket=%d\n", ev_fd);
                pSocket->OnWrite();
            }

            if (events[i].events & (EPOLLPRI | EPOLLERR | EPOLLHUP))
            {
                // printf("OnClose, socket=%d\n", ev_fd);
                pSocket->OnClose();
            }

            pSocket->ReleaseRef();
        }

        _CheckTimer();
        _CheckLoop();
    }
}

void EventDispatch::StopDispatch()
{
    //能否平滑退出
    running = false;
    close(m_epfd);
}
