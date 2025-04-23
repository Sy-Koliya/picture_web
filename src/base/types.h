#ifndef TYPE_H
#define TYPE_H
#include <cstdint>
#include <functional>
#include "tools.h"

constexpr int SOCKET_ERROR = -1;
constexpr int _INVALID_SOCKET = -1;
constexpr int NETLIB_INVALID_HANDLE = -1;
using net_handle_t = int;
using conn_handle_t = int;

// SOCKET
enum
{
    SOCKET_STATE_IDLE,
    SOCKET_STATE_LISTENING,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSING
};

// epoll
enum
{
    SOCKET_READ = 0x1,
    SOCKET_WRITE = 0x2,
    SOCKET_EXCEP = 0x4,
    SOCKET_ALL = 0x7
};

// // CallbackStatus
// enum
// {
//     NETLIB_MSG_CONNECT = 1,
//     NETLIB_MSG_CONFIRM,
//     NETLIB_MSG_READ,
//     NETLIB_MSG_WRITE,
//     NETLIB_MSG_CLOSE,
//     NETLIB_MSG_TIMER,
//     NETLIB_MSG_LOOP
// };

// NetStat
enum
{
    NETLIB_OK = 0,
    NETLIB_ERROR = -1
};

enum class HttpState : uint32_t {
    Http_Header_Read     = 1u << 0,  
    Http_Header_Parser   = 1u << 1,  
    Http_Len_Parser      = 1u << 2, 
    Http_Chunked_Parser  = 1u << 3,
    Http_Body_Parser     = 1u << 4,  
    HttpCallback         = 1u << 5,  
    Http_Ready           = 1u << 6,  
    Http_Error           = 1u << 7
};

class NoCopy
{
public:
    NoCopy() = default;
    ~NoCopy() = default;

    // 禁止拷贝
    NoCopy(const NoCopy &) = delete;
    NoCopy &operator=(const NoCopy &) = delete;
};
//typedef void (*callback_t)(void *callbackdata, uint8_t msg, uint32_t handle, void *pParam);
// using callbackt = std::function<void()>;

#endif