#include "BaseSocket.h"
#include "EventDispatch.h"
#include "ThrdPool.h"
#include <sys/socket.h>  // 定义 socket()、bind() 等
#include <arpa/inet.h>   // 定义 inet_pton()/inet_ntop() 等地址转换函数
#include <unistd.h>      //定义 close()等系统调用
#include <fcntl.h>
#include <string.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/ioctl.h>
#include <netdb.h> //for dns
#include <map>
#include <sys/epoll.h>
 

typedef std::map<net_handle_t, BaseSocket*> SocketMap;
SocketMap	g_socket_map;

void AddBaseSocket(BaseSocket* pSocket)
{
	g_socket_map.insert(std::make_pair((net_handle_t)pSocket->GetSocket(), pSocket));
}

void RemoveBaseSocket(BaseSocket* pSocket)
{
	g_socket_map.erase((net_handle_t)pSocket->GetSocket());
}

BaseSocket* FindBaseSocket(net_handle_t fd)
{
	BaseSocket* pSocket = nullptr;
	SocketMap::iterator iter = g_socket_map.find(fd);
	if (iter != g_socket_map.end())
	{
		pSocket = iter->second;
	}

	return pSocket;
}

//////////////////////////////

BaseSocket::BaseSocket()
{
	//printf("BaseSocket::BaseSocket\n");
	m_socket = _INVALID_SOCKET;
	m_state = SOCKET_STATE_IDLE;
	in_buf= buffer_new(1);
	out_buf = buffer_new(1);
}

BaseSocket::~BaseSocket()
{
	//printf("BaseSocket::~BaseSocket, socket=%d\n", m_socket);
}

int BaseSocket::Listen(const char* server_ip, uint16_t port, callback_t callback, void* callback_data)
{
	m_local_ip = server_ip;
	m_local_port = port;
	m_callback = callback;
	m_callback_data = callback_data;

	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket == _INVALID_SOCKET)
	{
		printf("socket failed, err_code=%d, server_ip=%s, port=%u", _GetErrorCode(), server_ip, port);
		return NETLIB_ERROR;
	}

	_SetReuseAddr(m_socket);
	_SetNonblock(m_socket);

	sockaddr_in serv_addr;
	_SetAddr(server_ip, port, &serv_addr);
    int ret = ::bind(m_socket, (sockaddr*)&serv_addr, sizeof(serv_addr));
	if (ret == SOCKET_ERROR)
	{
        printf("bind failed, err_code=%d, server_ip=%s, port=%u", _GetErrorCode(), server_ip, port);
		close(m_socket);
		return NETLIB_ERROR;
	}

	ret = listen(m_socket, 64);
	if (ret == SOCKET_ERROR)
	{
        printf("listen failed, err_code=%d, server_ip=%s, port=%u", _GetErrorCode(), server_ip, port);
		close(m_socket);
		return NETLIB_ERROR;
	}

	m_state = SOCKET_STATE_LISTENING;

	printf("BaseSocket::Listen on %s:%d", server_ip, port);

	AddBaseSocket(this);
	SocketPool::Instance().AddSocketEvent(m_socket, EPOLLIN);
	return NETLIB_OK;
}

net_handle_t BaseSocket::Connect(const char* server_ip, uint16_t port, callback_t callback, void* callback_data)
{
	printf("BaseSocket::Connect, server_ip=%s, port=%d", server_ip, port);

	m_remote_ip = server_ip;
	m_remote_port = port;
	m_callback = callback;
	m_callback_data = callback_data;

	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket == _INVALID_SOCKET)
	{
        printf("socket failed, err_code=%d, server_ip=%s, port=%u", _GetErrorCode(), server_ip, port);
		return NETLIB_INVALID_HANDLE;
	}

	_SetNonblock(m_socket);
	_SetNoDelay(m_socket);
	sockaddr_in serv_addr;
	_SetAddr(server_ip, port, &serv_addr);
	int ret = connect(m_socket, (sockaddr*)&serv_addr, sizeof(serv_addr));
	if ( (ret == SOCKET_ERROR) && (!_IsBlock(_GetErrorCode())) )
	{	
        printf("connect failed, err_code=%d, server_ip=%s, port=%u", _GetErrorCode(), server_ip, port);
		close(m_socket);
		return NETLIB_INVALID_HANDLE;
	}
	m_state = SOCKET_STATE_CONNECTING;
	AddBaseSocket(this);
	SocketPool::Instance().AddSocketEvent(m_socket, EPOLLOUT);
	return (net_handle_t)m_socket;
}

int BaseSocket::Send(void* buf, int len)
{
	if (m_state != SOCKET_STATE_CONNECTED)
		return NETLIB_ERROR;

	int ret = send(m_socket, (char*)buf, len, 0);
	if (ret == SOCKET_ERROR)
	{
		int err_code = _GetErrorCode();
		if (_IsBlock(err_code))
		{
			ret = 0;
			//printf("socket send block fd=%d", m_socket);
		}
		else
		{
            printf("send failed, err_code=%d, len=%d", err_code, len);
		}
	}

	return ret;
}

int BaseSocket::Recv(void* buf, int len)
{
	return recv(m_socket, (char*)buf, len, 0);
}

int BaseSocket::Close()
{
	if(m_ev_dispatch!=nullptr)m_ev_dispatch->RemoveEvent(m_socket);
	else{
		//loginfo
	}
	RemoveBaseSocket(this);
	close(m_socket);

	return 0;
}
void BaseSocket::OnRead()
{
	if (m_state == SOCKET_STATE_LISTENING)
	{
		_AcceptNewSocket();
	}
	else
	{
		u_long avail = 0;
        int ret = ioctl(m_socket, FIONREAD, &avail);
		if ( (SOCKET_ERROR == ret) || (avail == 0) )
		{
			m_callback(m_callback_data, NETLIB_MSG_CLOSE, (net_handle_t)m_socket, nullptr);
		}
		else
		{
			m_callback(m_callback_data, NETLIB_MSG_READ, (net_handle_t)m_socket, nullptr);
		}
	}
}
void BaseSocket::OnWrite()
{

	if (m_state == SOCKET_STATE_CONNECTING)
	{
		int error = 0;
		socklen_t len = sizeof(error);
		getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (void*)&error, &len);
		if (error) {
			m_callback(m_callback_data, NETLIB_MSG_CLOSE, (net_handle_t)m_socket, nullptr);
		} else {
			m_state = SOCKET_STATE_CONNECTED;
			m_callback(m_callback_data, NETLIB_MSG_CONFIRM, (net_handle_t)m_socket, nullptr);
		}
	}
	else
	{
		m_callback(m_callback_data, NETLIB_MSG_WRITE, (net_handle_t)m_socket, nullptr);
	}
}
void BaseSocket::OnClose()
{
	m_state = SOCKET_STATE_CLOSING;
	m_callback(m_callback_data, NETLIB_MSG_CLOSE, (net_handle_t)m_socket, nullptr);
}

void BaseSocket::SetSendBufSize(uint32_t send_size)
{
	int ret = setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &send_size, 4);
	if (ret == SOCKET_ERROR) {
        printf("set SO_SNDBUF failed for fd=%d", m_socket);
	}

	socklen_t len = 4;
	int size = 0;
	getsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &size, &len);
	printf("socket=%d send_buf_size=%d", m_socket, size);
}

void BaseSocket::SetRecvBufSize(uint32_t recv_size)
{
	int ret = setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &recv_size, 4);
	if (ret == SOCKET_ERROR) {
        printf("set SO_RCVBUF failed for fd=%d", m_socket);
	}

	socklen_t len = 4;
	int size = 0;
	getsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, &size, &len);
	printf("socket=%d recv_buf_size=%d", m_socket, size);
}

int BaseSocket::_GetErrorCode()
{
	return errno;
}

bool BaseSocket::_IsBlock(int error_code)
{
	return ( (error_code == EINPROGRESS) || (error_code == EWOULDBLOCK) );
}

void BaseSocket::_SetNonblock(int fd)
{
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL));
	if (ret == SOCKET_ERROR)
	{
        printf("_SetNonblock failed, err_code=%d, fd=%d", _GetErrorCode(), fd);
	}
}

void BaseSocket::_SetReuseAddr(int fd)
{
	int reuse = 1;
	int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
	if (ret == SOCKET_ERROR)
	{
        printf("_SetReuseAddr failed, err_code=%d, fd=%d", _GetErrorCode(), fd);
	}
}

void BaseSocket::_SetNoDelay(int fd)
{
	int nodelay = 1;
	int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
	if (ret == SOCKET_ERROR)
	{
        printf("_SetNoDelay failed, err_code=%d, fd=%d", _GetErrorCode(), fd);
	}
}

void BaseSocket::_SetAddr(const char* ip, const uint16_t port, sockaddr_in* pAddr)
{
	memset(pAddr, 0, sizeof(sockaddr_in));
	pAddr->sin_family = AF_INET;
	pAddr->sin_port = htons(port);
	pAddr->sin_addr.s_addr = inet_addr(ip);
	// 	  如果 inet_addr 返回 INADDR_NONE（-1），说明 ip 不是一个合法的 "a.b.c.d"，
    //    可能是一个域名，就用 DNS 解析
	if (pAddr->sin_addr.s_addr == INADDR_NONE)
	{
		hostent* host = gethostbyname(ip);
		if (host == nullptr)
		{
            printf("gethostbyname failed, ip=%s, port=%u", ip, port);
			return;
		}
		pAddr->sin_addr.s_addr = *(uint32_t*)host->h_addr;
	}
}
BaseSocket* BaseSocket::SetNewSession(){
	BaseSocket* pSocket = new BaseSocket();
	pSocket->SetCallback(m_callback);
	pSocket->SetCallbackData(m_callback_data);
}
void BaseSocket::_AcceptNewSocket()
{
	int fd = 0;
	sockaddr_in peer_addr;
	socklen_t addr_len = sizeof(sockaddr_in);
	char ip_str[INET_ADDRSTRLEN];
	while ( (fd = accept(m_socket, (sockaddr*)&peer_addr, &addr_len)) != _INVALID_SOCKET )
	{
		uint16_t port = ntohs(peer_addr.sin_port);
		
		if (inet_ntop(AF_INET, &peer_addr.sin_addr, ip_str, sizeof(ip_str)) == nullptr) {
			perror("inet_ntop");
			return;
		}
		printf("AcceptNewSocket, socket=%d from %s:%d\n", fd, ip_str, port);
		

		//new_session
		BaseSocket* pSocket = SetNewSession();

		pSocket->SetSocket(fd);
		pSocket->SetState(SOCKET_STATE_CONNECTED);
		pSocket->SetRemoteIP(ip_str);
		pSocket->SetRemotePort(port);

		_SetNoDelay(fd);
		_SetNonblock(fd);
		AddBaseSocket(pSocket);
		SocketPool::Instance().AddSocketEvent(fd, SOCKET_READ | SOCKET_EXCEP);
		m_callback(m_callback_data, NETLIB_MSG_CONNECT, (net_handle_t)fd, nullptr);
	}
}

