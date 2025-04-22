
#ifndef __SOCKET_H__
#define __SOCKET_H__
 
#include "buffer.h"
#include "types.h"
#include "tools.h"
#include "EventDispatch.h"
#include <netinet/in.h>
#include <string>
#include <functional>


template <class Dervied>
class Base {
protected:
 
    void  OnRead(){
        // 调用子类的实现函数。要求子类必须实现名为 impl() 的函数
        // 这个函数会被调用来执行具体的操作
        static_cast<Dervied*>(this)->read_impl();
    }

    void OnWrite(){
        static_cast<Dervied*>(this)->write_impl();
    }
    void OnClose(){
        static_cast<Dervied*>(this)->close_impl();
    }

     void _AcceptNewSocket(){
        static_cast<Derived*>(this)->accept_impl();
     }
};


//template <class Dervied>
 class BaseSocket
 {
    //using callback_t = std::function<void()>; 

 public:
     BaseSocket();
 
     virtual ~BaseSocket();
 
     int GetSocket() { return m_socket; }
     void SetEventDispatch(EventDispatch* ed) { m_ev_dispatch = ed; }
     EventDispatch* GetEventDispatch(){return m_ev_dispatch;}
     void SetSocket(int  fd) { m_socket = fd; }
     void SetState(uint8_t state) { m_state = state; }
 
     void SetCallback(callback_t callback) { m_callback = callback; }
     void SetCallbackData(void *data) { m_callback_data = data; }
     void SetRemoteIP(char *ip) { m_remote_ip = ip; }
     void SetRemotePort(uint16_t port) { m_remote_port = port; }
     void SetSendBufSize(uint32_t send_size);
     void SetRecvBufSize(uint32_t recv_size);


     const char *GetRemoteIP() { return m_remote_ip.c_str(); }
     uint16_t GetRemotePort() { return m_remote_port; }
     const char *GetLocalIP() { return m_local_ip.c_str(); }
     uint16_t GetLocalPort() { return m_local_port; }
 
 public:
     int Listen(
         const char *server_ip,
         uint16_t port,
         callback_t callback,
         void *callback_data);
 
     net_handle_t Connect(
         const char *server_ip,
         uint16_t port,
         callback_t callback,
         void *callback_data);
 
     int Send(void *buf, int len);
 
     int Recv(void *buf, int len);
 
     int Close();
 
 public:
    void  OnRead();
    void OnWrite();
    void OnClose();
 protected:
     BaseSocket* SetNewSession();
     void Close_imp();
     void Read_imp();
     void Write_imp();
 private:
     int _GetErrorCode();
     bool _IsBlock(int error_code);
 
     void _SetNonblock(int fd);
     void _SetReuseAddr(int fd);
     void _SetNoDelay(int fd);
     void _SetAddr(const char *ip, const uint16_t port, sockaddr_in *pAddr);
 
     void _AcceptNewSocket();
 
 private:
     std::string m_remote_ip;
     uint16_t m_remote_port;
     std::string m_local_ip;
     uint16_t m_local_port;

     callback_t m_callback;
     void *m_callback_data;

     buffer_t* in_buf;
     buffer_t* out_buf;

     uint8_t m_state;


     EventDispatch* m_ev_dispatch;
     int m_socket;
 };
 
 BaseSocket *FindBaseSocket(net_handle_t fd);
 
 #endif
 