#include "BaseSocket.h"
#include "EventDispatch.h"
#include "ThrdPool.h"
#include <sys/socket.h> // 定义 socket()、bind() 等
#include <arpa/inet.h>  // 定义 inet_pton()/inet_ntop() 等地址转换函数
#include <unistd.h>     //定义 close()等系统调用
#include <fcntl.h>
#include <string.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/ioctl.h>
#include <netdb.h> //for dns
#include <map>
#include <sys/epoll.h>
#include <iostream>
#include <exception>


BaseCount::BaseCount(BaseSocket* p)
  : ptr(p)
{
    if (ptr) {
        ptr->acquire();
    }
}

BaseCount::BaseCount(const BaseCount& other)
  : ptr(other.ptr)
{
    if (ptr) {
        ptr->acquire();
    }
}

BaseCount& BaseCount::operator=(const BaseCount& other)
{
    if (this != &other) {
        if (ptr) {
            ptr->release();
        }
        ptr = other.ptr;
        if (ptr) {
            ptr->acquire();
        }
    }
    return *this;
}

BaseCount::BaseCount(BaseCount&& other) noexcept
  : ptr(other.ptr)
{
    other.ptr = nullptr;
}

BaseCount& BaseCount::operator=(BaseCount&& other) noexcept
{
    if (this != &other) {
        if (ptr) {
            ptr->release();
        }
        ptr = other.ptr;
        other.ptr = nullptr;
    }
    return *this;
}

BaseCount::~BaseCount()
{
    if (ptr) {
        ptr->release();
    }
}

BaseSocket* BaseCount::GetBasePtr() const
{
    return ptr;
}

typedef std::map<net_handle_t, BaseSocket *> SocketMap;
SocketMap g_socket_map;
static std::mutex g_map_mutex;

void AddBaseSocket(BaseSocket *pSocket)
{
    std::lock_guard<std::mutex> lk(g_map_mutex);
    g_socket_map.insert(std::make_pair((net_handle_t)pSocket->GetSocket(), pSocket));
}

void RemoveBaseSocket(BaseSocket *pSocket)
{
    std::lock_guard<std::mutex> lk(g_map_mutex);
    g_socket_map.erase((net_handle_t)pSocket->GetSocket());
}

BaseCount FindBaseSocket(net_handle_t fd)
{
    std::lock_guard<std::mutex> lk(g_map_mutex);
    BaseSocket *pSocket = nullptr;
    SocketMap::iterator iter = g_socket_map.find(fd);
    if (iter != g_socket_map.end())
    {
        pSocket = iter->second;
    }

    return BaseCount(pSocket);
}

BaseSocket::BaseSocket()
{
    m_socket = _INVALID_SOCKET;
    m_state = SOCKET_STATE_IDLE;
    ref = 1;
    isptr = true;
    in_buf = buffer_new(1);
}

BaseSocket::~BaseSocket()
{
    // if(Global::Instance().get<int>("Debug") & 1)
    // std::cout << "m_socket " << m_socket << "  closed" << '\n';
    buffer_free(in_buf);
}
////////////////////////

int BaseSocket::Read_imp()
{

    return 0;
}

int BaseSocket::Write_imp()
{
    return 0;
}
int BaseSocket::Listen_imp()
{
    return 0;
}
int BaseSocket::Connect_imp()
{
    return 0;
}

int BaseSocket::Close_imp()
{
    return 0;
}

BaseSocket *BaseSocket::AddNew_imp()
{
    return new BaseSocket{};
}

/////////////////////////

int BaseSocket::Listen(const char *server_ip, uint16_t port)
{
    m_local_ip = server_ip;
    m_local_port = port;
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == _INVALID_SOCKET)
    {
        std::cerr
            << "socket failed, err_code=" << _GetErrorCode()
            << ", server_ip=" << server_ip
            << ", port=" << port
            << "\n";
        return NETLIB_ERROR;
    }

    _SetReuseAddr(m_socket);
    _SetNonblock(m_socket);

    sockaddr_in serv_addr;
    _SetAddr(server_ip, port, &serv_addr);
    int ret = ::bind(m_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == SOCKET_ERROR)
    {
        std::cerr
            << "bind failed, err_code=" << _GetErrorCode()
            << ", server_ip=" << server_ip
            << ", port=" << port
            << "\n";
        close(m_socket);
        return NETLIB_ERROR;
    }

    ret = listen(m_socket, 64);
    if (ret == SOCKET_ERROR)
    {
        std::cerr
            << "listen failed, err_code=" << _GetErrorCode()
            << ", server_ip=" << server_ip
            << ", port=" << port
            << "\n";
        close(m_socket);
        return NETLIB_ERROR;
    }

    m_state = SOCKET_STATE_LISTENING;

    std::cout
        << "BaseSocket::Listen on "
        << server_ip << ":" << port
        << std::endl;

    AddBaseSocket(this);
    SocketPool::Instance().AddSocketEvent(m_socket, EPOLLIN);
    Listen_imp();
    return NETLIB_OK;
}

net_handle_t BaseSocket::Connect(const char *server_ip, uint16_t port)
{
    std::cout
        << "BaseSocket::Connect, server_ip=" << server_ip
        << ", port=" << port
        << "\n";

    m_remote_ip = server_ip;
    m_remote_port = port;

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == _INVALID_SOCKET)
    {
        std::cerr
            << "socket failed, err_code=" << _GetErrorCode()
            << ", server_ip=" << server_ip
            << ", port=" << port
            << "\n";
        return NETLIB_INVALID_HANDLE;
    }

    _SetNonblock(m_socket);
    _SetNoDelay(m_socket);
    sockaddr_in serv_addr;
    _SetAddr(server_ip, port, &serv_addr);
    int ret = connect(m_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
    if ((ret == SOCKET_ERROR) && (!_IsBlock(_GetErrorCode())))
    {
        std::cerr
            << "connect failed, err_code=" << _GetErrorCode()
            << ", server_ip=" << server_ip
            << ", port=" << port
            << "\n";
        close(m_socket);
        return NETLIB_INVALID_HANDLE;
    }

    m_state = SOCKET_STATE_CONNECTING;
    AddBaseSocket(this);
    SocketPool::Instance().AddSocketEvent(m_socket, EPOLLOUT);

    Connect_imp();
    return (net_handle_t)m_socket;
}

int BaseSocket::Send(void *buf, int len)
{
    if (m_state != SOCKET_STATE_CONNECTED)
        return NETLIB_ERROR;

    int ret = ::send(m_socket, static_cast<char *>(buf), len, 0);
    if (ret == SOCKET_ERROR)
    {
        int err_code = _GetErrorCode();
        if (_IsBlock(err_code))
        {
            // 非阻塞模式下缓冲区已满，返回 0 表示稍后再试
            ret = 0;
        }
        else
        {
            if (Global::Instance().get<int>("Debug") & 1)
                std::cerr
                    << "send failed, err_code=" << err_code
                    << ", len=" << len
                    << "\n";
        }
    }

    return ret;
}

int BaseSocket::Recv(void *buf, int len)
{
    return recv(m_socket, (char *)buf, len, 0);
}

void BaseSocket::acquire()
{
    ref++;
}

void BaseSocket::release()
{
    ref--;
    if (ref == 0)
    {
        if (isptr)
            delete this;
    }
}

int BaseSocket::Close()
{
    std::lock_guard<std::mutex> lk(b_lock);
    if (m_state == SOCKET_STATE_CLOSED)
    {
        return 1;
    }
    m_state = SOCKET_STATE_CLOSED;
    Close_imp();
    if (m_ev_dispatch != nullptr)
        m_ev_dispatch->RemoveEvent(m_socket);
    RemoveBaseSocket(this);
    ::close(m_socket);
    release();
    return 0;
}

//On 方法都在同一个dispatch线程中调用，不会出现竞态
void BaseSocket::OnRead()
{
    if (m_state == SOCKET_STATE_CLOSED)
        return;
    if (m_state == SOCKET_STATE_LISTENING)
    {
        _AcceptNewSocket();
    }
    else
    {
        u_long avail = 0;
        int ret = ioctl(m_socket, FIONREAD, &avail);
        if ((SOCKET_ERROR == ret) || (avail == 0))
        {
            OnClose();
        }
        else
        {
            //std::lock_guard<std::mutex>lk(b_lock);
            char tmp_buf[4096];
            int len = recv(m_socket, tmp_buf, sizeof(tmp_buf), 0);
            buffer_add(in_buf, tmp_buf, len);
            Read_imp();
        }
    }
}
void BaseSocket::OnWrite()
{
    if (m_state == SOCKET_STATE_CLOSED)
        return;
    if (m_state == SOCKET_STATE_CONNECTING)
    {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (void *)&error, &len);
        if (error)
        {
            OnClose();
        }
        else
        {
            m_state = SOCKET_STATE_CONNECTED;
        }
    }
    else
    {
        //std::lock_guard<std::mutex>lk(b_lock);
        Write_imp();
    }
}

void BaseSocket::OnClose()
{
    Close();
}

void BaseSocket::SetSendBufSize(uint32_t send_size)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &send_size, sizeof(send_size)) == SOCKET_ERROR)
    {
        std::cerr
            << "set SO_SNDBUF failed for fd=" << m_socket
            << ", err_code=" << _GetErrorCode()
            << "\n";
    }

    socklen_t len = sizeof(send_size);
    int actual_size = 0;
    getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &actual_size, &len);
    std::cout
        << "socket=" << m_socket
        << " send_buf_size=" << actual_size
        << "\n";
}

void BaseSocket::SetRecvBufSize(uint32_t recv_size)
{
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &recv_size, sizeof(recv_size)) == SOCKET_ERROR)
    {
        std::cerr
            << "set SO_RCVBUF failed for fd=" << m_socket
            << ", err_code=" << _GetErrorCode()
            << "\n";
    }

    socklen_t len = sizeof(recv_size);
    int actual_size = 0;
    getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &actual_size, &len);
    std::cout
        << "socket=" << m_socket
        << " recv_buf_size=" << actual_size
        << "\n";
}

int BaseSocket::_GetErrorCode()
{
    return errno;
}

bool BaseSocket::_IsBlock(int error_code)
{
    return ((error_code == EINPROGRESS) || (error_code == EWOULDBLOCK));
}

void BaseSocket::_SetNonblock(int fd)
{
    int ret = fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL));
    if (ret == SOCKET_ERROR)
    {
        std::cerr
            << "_SetNonblock failed, err_code=" << _GetErrorCode()
            << ", fd=" << fd
            << "\n";
    }
}
void BaseSocket::_SetReuseAddr(int fd)
{
    int reuse = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret == SOCKET_ERROR)
    {
        std::cerr
            << "_SetReuseAddr failed, err_code=" << _GetErrorCode()
            << ", fd=" << fd
            << "\n";
    }
}

void BaseSocket::_SetNoDelay(int fd)
{
    int nodelay = 1;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    if (ret == SOCKET_ERROR)
    {
        std::cerr
            << "_SetNoDelay failed, err_code=" << _GetErrorCode()
            << ", fd=" << fd
            << "\n";
    }
}

void BaseSocket::_SetAddr(const char *ip, const uint16_t port, sockaddr_in *pAddr)
{
    memset(pAddr, 0, sizeof(*pAddr));
    pAddr->sin_family = AF_INET;
    pAddr->sin_port = htons(port);
    pAddr->sin_addr.s_addr = inet_addr(ip);

    // 如果 inet_addr 返回 INADDR_NONE，说明不是 a.b.c.d 格式，尝试 DNS 解析
    if (pAddr->sin_addr.s_addr == INADDR_NONE)
    {
        hostent *host = gethostbyname(ip);
        if (host == nullptr)
        {
            std::cerr
                << "gethostbyname failed, ip=" << ip
                << ", port=" << port
                << "\n";
            return;
        }
        pAddr->sin_addr.s_addr = *reinterpret_cast<uint32_t *>(host->h_addr);
    }
}

void BaseSocket::_AcceptNewSocket()
{
    int fd = 0;
    sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(sockaddr_in);
    char ip_str[INET_ADDRSTRLEN];
    while ((fd = accept(m_socket, (sockaddr *)&peer_addr, &addr_len)) != _INVALID_SOCKET)
    {
        uint16_t port = ntohs(peer_addr.sin_port);

        if (inet_ntop(AF_INET, &peer_addr.sin_addr, ip_str, sizeof(ip_str)) == nullptr)
        {
            perror("inet_ntop");
            return;
        }
        if(Global::Instance().get<int>("Debug") & 1)
        std::cout
            << "AcceptNewSocket, socket=" << fd
            << " from " << ip_str
            << ":" << port
            << "\n";

        // new_session
        BaseSocket *pSocket = AddNew_imp();
        ;
        pSocket->SetSocket(fd);
        pSocket->SetState(SOCKET_STATE_CONNECTED);
        pSocket->SetRemoteIP(ip_str);
        pSocket->SetRemotePort(port);

        _SetNoDelay(fd);
        _SetNonblock(fd);
        AddBaseSocket(pSocket);
        SocketPool::Instance().AddSocketEvent(fd, EPOLLIN);
    }
}
