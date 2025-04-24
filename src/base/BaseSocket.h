
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

// template <class Dervied>
// class Base {
// protected:
 
//     void  OnRead(){
//         // 调用子类的实现函数。要求子类必须实现名为 impl() 的函数
//         // 这个函数会被调用来执行具体的操作
//         static_cast<Dervied*>(this)->read_impl();
//     }

//     void OnWrite(){
//         static_cast<Dervied*>(this)->write_impl();
//     }
//     void OnClose(){
//         static_cast<Dervied*>(this)->close_impl();
//     }

//      void _AcceptNewSocket(){
//         static_cast<Derived*>(this)->accept_impl();
//      }
// };


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
         uint16_t port
        );
 
     net_handle_t Connect(
         const char *server_ip,
         uint16_t port
        );
 
     int Send(void *buf, int len);
 
     int Recv(void *buf, int len);
 
     int Close();
 
 public:
    void OnRead();
    void OnWrite();
    void OnClose();
 protected:
     virtual int Close_imp();
     virtual int Read_imp();
     virtual int Write_imp();
     virtual int Listen_imp();
     virtual int Connect_imp();
     virtual BaseSocket* AddNew_imp();
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

        
     buffer_t* in_buf;
     buffer_t* out_buf;
     uint8_t m_state;


     EventDispatch* m_ev_dispatch;
     int m_socket;
 };
 
 
 BaseSocket *FindBaseSocket(net_handle_t fd);
 

 class BaseSocketManager : public NoCopy
 {
 public:
     static BaseSocketManager &Instance()
     {
         static BaseSocketManager inst;
         return inst;
     }
 
     BaseSocket *Create()
     {
         std::lock_guard<std::mutex> guard(m_lock);
         BaseSocket *socket_ptr = new BaseSocket();
         socket_handle_.insert(socket_ptr);
         return socket_ptr;
     }
 
     void Destroy(BaseSocket *socket_ptr)
     {
         std::lock_guard<std::mutex> guard(m_lock);
         if (!socket_ptr)
             return;
         auto it = socket_handle_.find(socket_ptr);
         if (it != socket_handle_.end())
         {
             socket_ptr->Close();
             delete socket_ptr;
             socket_handle_.erase(it);
         }
         else
         {
             std::cout << "No tracked socket_ptr found!" << std::endl;
         }
     }
 
 private:
     BaseSocketManager() = default;
     ~BaseSocketManager()
     {
         for (auto socket_ptr : socket_handle_)
         {
             if (socket_ptr)
             {
                 socket_ptr->Close();
                 //delete socket_ptr; 双重释放！
             }
         }
     }
 
     mutable std::mutex m_lock;
     std::unordered_set<BaseSocket *> socket_handle_;
 };

 #endif
 