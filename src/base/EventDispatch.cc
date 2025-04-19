#include "EventDispatch.h"
#include "BaseSocket.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

#define MIN_TIMER_DURATION 100 // 100 miliseconds

CEventDispatch *CEventDispatch::m_pEventDispatch = nullptr;

CEventDispatch::CEventDispatch()
{
    running = false;
    m_epfd = epoll_create(1024);
    if (m_epfd == -1)
    {
        printf("epoll_create failed");
    }
}

CEventDispatch::~CEventDispatch()
{
    close(m_epfd);
}

// 注册计时事件并且返回句柄
int CEventDispatch::AddTimer(TimerEvent *te)
{
    m_timer_list.push(te);
    return te->te_id;
}

int CEventDispatch::AddTimer(callback_t callback, void *user_data, uint64_t interval, int calltime = -1)
{
    TimerEvent *te = (TimerEvent *)malloc(sizeof(TimerEvent));
    te->callback = callback;
    te->user_data = user_data;
    te->interval = interval;
    te->next_tick = get_tick_count();
    te->te_id = id_helper.Get();
    te->calltime = calltime;
    handle_te.insert(std::make_pair(te->te_id, te));
    return AddTimer(te);
}

void CEventDispatch::RemoveTimer(int handle_te_id)
{
    if (handle_te.find(handle_te_id) == handle_te.end())
        return;
    TimerEvent *te = handle_te[handle_te_id];
    delete te;
    handle_te.erase(handle_te_id);
    id_helper.Del(handle_te_id);
}

void CEventDispatch::_CheckTimer()
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
            te->callback(te->user_data, NETLIB_MSG_TIMER, 0, nullptr);  //此处后面加入线程池
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
            break;
    }
}

void CEventDispatch::AddLoop(callback_t callback, void *user_data)
{
    TimerEvent *te = (TimerEvent *)malloc(sizeof(TimerEvent));
    te->callback = callback;
    te->user_data = user_data;
    te->interval = -1;
    te->next_tick = -1;
    te->te_id = id_helper.Get();
    te->calltime = -2;
    m_loop_list.push_back(te);
}

void CEventDispatch::_CheckLoop()
{
    for(auto it:m_loop_list){
        it->callback(it->user_data, NETLIB_MSG_LOOP, 0, nullptr);
    }
}

CEventDispatch& CEventDispatch::Instance()
{
    static CEventDispatch  m_pEventDispatch;
    return m_pEventDispatch;
}

void CEventDispatch::AddEvent(int fd, uint8_t socket_event)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLPRI | EPOLLERR | EPOLLHUP;
    ev.data.fd = fd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        printf("epoll_ctl() failed, errno=%d", errno);
    }
}

void CEventDispatch::RemoveEvent(int fd, uint8_t socket_event)
{
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) != 0)
    {
        printf("epoll_ctl failed, errno=%d", errno);
    }
}

void CEventDispatch::StartDispatch(uint32_t wait_timeout)
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
            CBaseSocket *pSocket = FindBaseSocket(ev_fd);
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

void CEventDispatch::StopDispatch()
{
    running = false;
}
