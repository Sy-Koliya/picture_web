
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "buffer.h"
#include "types.h"
#include "tools.h"
#include "EventDispatch.h"
#include <netinet/in.h>
#include <string>
#include <functional>
#include <iostream>
#include <atomic>
#include <mutex>

class BaseSocket
{

public:
    int GetSocket() { return m_socket; }
    void SetEventDispatch(EventDispatch *ed) { m_ev_dispatch = ed; }
    EventDispatch *GetEventDispatch() { return m_ev_dispatch; }
    void SetSocket(int fd) { m_socket = fd; }
    void SetState(uint8_t state) { m_state = state; }

    void SetRemoteIP(char *ip) { m_remote_ip = ip; }
    void SetRemotePort(uint16_t port) { m_remote_port = port; }
    void SetSendBufSize(uint32_t send_size);
    void SetRecvBufSize(uint32_t recv_size);

    const char *GetRemoteIP() { return m_remote_ip.c_str(); }
    uint16_t GetRemotePort() { return m_remote_port; }
    const char *GetLocalIP() { return m_local_ip.c_str(); }
    uint16_t GetLocalPort() { return m_local_port; }
    uint8_t GetState() { return m_state; }
    bool IsAlive() { return m_state != SOCKET_STATE_CLOSED; }

public:
    int Listen(
        const char *server_ip,
        uint16_t port);

    net_handle_t Connect(
        const char *server_ip,
        uint16_t port);

    int Send(void *buf, int len);

    int Recv(void *buf, int len);

    int Close();

    void acquire();
    void release();

protected:
    void OnRead();
    void OnWrite();
    void OnClose();

protected:
    BaseSocket();
    virtual ~BaseSocket();
    virtual int Close_imp();
    virtual int Read_imp();
    virtual int Write_imp();
    virtual int Listen_imp();
    virtual int Connect_imp();
    virtual BaseSocket *AddNew_imp();

private:
    friend class BaseSocketManager;
    friend void EventDispatch::StartDispatch(uint32_t);

private:
    int _GetErrorCode();
    bool _IsBlock(int error_code);

    void _SetNonblock(int fd);
    void _SetReuseAddr(int fd);
    void _SetNoDelay(int fd);
    void _SetAddr(const char *ip, const uint16_t port, sockaddr_in *pAddr);

    void _AcceptNewSocket();

protected:
    std::string m_remote_ip;
    uint16_t m_remote_port;
    std::string m_local_ip;
    uint16_t m_local_port;

    buffer_t *in_buf;
    buffer_t *out_buf;
    std::atomic<int> m_state;

    EventDispatch *m_ev_dispatch;
    int m_socket;
    bool isptr = true;
    std::atomic<int> ref;
    std::mutex b_lock;
};


class BaseCount {
    public:

        explicit BaseCount(BaseSocket* p = nullptr);

        BaseCount(const BaseCount& other);

        BaseCount& operator=(const BaseCount& other);

        BaseCount(BaseCount&& other) noexcept;

        BaseCount& operator=(BaseCount&& other) noexcept;
    
        ~BaseCount();
    
        BaseSocket* GetBasePtr() const;
        operator bool() const noexcept { return ptr != nullptr; }
    private:
        BaseSocket* ptr;
    };
BaseCount
FindBaseSocket(net_handle_t fd);

#endif
