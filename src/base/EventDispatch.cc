#include "EventDispatch.h"
#include "BaseSocket.h"
#include "ThrdPool.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>
#include <memory>

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
    std::lock_guard<std::mutex>lk(m_lock);
    handle_te.insert(te);
    m_timer_list.push(te);
}

void EventDispatch::RemoveTimer(TimerEvent *te)
{
    std::lock_guard<std::mutex>lk(m_lock);
    auto it = handle_te.find(te);
    if (it == handle_te.end())
        return;
    handle_te.erase(it);
}

void EventDispatch::_CheckTimer()
{
    uint64_t curr_tick = get_tick_count();
    while (!m_timer_list.empty())
    {
        TimerEvent *te = m_timer_list.top();
        m_timer_list.pop();
        if (te == nullptr)
        {
            continue;
        }else{
            std::lock_guard<std::mutex>lk(m_lock);
            auto it = handle_te.find(te);
            if(it==handle_te.end()){
                delete te;
                continue;
            }
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
                RemoveTimer(te);
                delete te;
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

int EventDispatch::AddEvent(int fd, uint32_t socket_event)
{
    struct epoll_event ev;
    ev.events = socket_event;
    ev.data.fd = fd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        if (Global::Instance().get<int>("Debug") & 1)
        std::cerr
            << "epoll_ctl_add failed, errno=" << errno
            << "\n";
        return -1;
    }
    return 0;
}
int EventDispatch::ModifyEvent(int fd,uint32_t socket_event){
    struct epoll_event ev;
    ev.events = socket_event;
    ev.data.fd = fd;
    if(epoll_ctl(m_epfd,EPOLL_CTL_MOD,fd,&ev)!=0){
        if (Global::Instance().get<int>("Debug") & 1)
        std::cerr
        << "epoll_ctl_mod failed, errno=" << errno
        << "\n";
        return -1;
    }
    return 0;
}


int EventDispatch::RemoveEvent(int fd)
{
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) != 0)
    {
        if (Global::Instance().get<int>("Debug") & 1)
        std::cerr
            << "epoll_ctl remove failed, errno=" << errno
            << "\n";
        return -1;
    }
    return 0;
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
            auto bs = FindBaseSocket(ev_fd);
            BaseSocket* pSocket = bs.GetBasePtr(); 
            if (!pSocket)
                continue;
                
            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                pSocket->OnClose();
                goto End;
            }
            if (events[i].events & EPOLLIN)
            {
                pSocket->OnRead();
            }
            if (events[i].events & EPOLLOUT)
            {
                pSocket->OnWrite();
            }
            End:
            ;
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
