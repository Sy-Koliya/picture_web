#include "HttpConn.h"
#include "HttpServer.h"
#include "HttpRequestParser.h"
#include "Global.h"
#include "Api_dispatch.h"
#include <string.h>
#include <sys/epoll.h>

using namespace httpparser;


HttpConn::HttpConn()
    : BaseSocket(),
      state(HttpState::Http_Header_Read),
      last_recv(std::chrono::steady_clock::now()),
      isKeepAlive(false)
{
    // any other setup…
}

// Destructor
HttpConn::~HttpConn() = default;


static int DispatchHttpRequest(int fd ,Request &req);

// 每一次有读事件就会调用这个
int HttpConn::Read_imp()
{
    last_recv = std::chrono::steady_clock::now();
    char buff[recv_buf_len];
    int len = Recv(buff, recv_buf_len);
    if (len == 0)
    {
        Close();
        return 1;
    }
    // if (Global::Instance().get<int>("Debug") & 1)
    // {
    //     std::cout << "recv new msg:\n   " << buff << '\n';
    // }
    buffer_add(in_buf, buff, len);
    if (buffer_len(in_buf) >= max_recv_len)
    {
        // 可能被攻击，进一步处理
    }
    HandleRead();
    return 0;
}

int HttpConn::Connect_imp()
{
    return 0;
}

int HttpConn::Close_imp()
{   
    //如果正在处理请求
    if(state == HttpState::HttpCallback){

    }
    // 如果发生错误要告诉客户端
    if (state == HttpState::Http_Error)
    {
        // HttpManager::Instacn().create_404_responce(m_remote_ip.c_str,m_remote_port);
    }
    return 0;
}



int HttpConn::Write_imp()
{
    sended_size += Send((char *)resp.c_str() + sended_size, resp.size() - sended_size);
    if (sended_size == resp.size())
    {
        sended_size = 0;
        resp.clear();
        if (isKeepAlive)
        {
            state = HttpState::Http_Header_Parser;
            m_ev_dispatch->ModifyEvent(m_socket, EPOLLIN );
            //防止读完不会触发
            HandleRead();
        }
        else
        {
            Close();
        }
    }
    return 0;
}

//-1 表示错误
int HttpConn::SetResponse(std::string &&_resp)
{
    if (state != HttpState::HttpCallback)
        return -1;
    resp = std::move(_resp);
    sended_size = 0;
    m_ev_dispatch->ModifyEvent(m_socket, EPOLLIN | EPOLLOUT);
    return 0;
}

void HttpConn::HandleRead()
{

    switch (state)
    {
    case HttpState::Http_Header_Read:
    {
        // 查找 "\r\n\r\n" 头结束标志
        const char *sep = "\r\n\r\n";
        int header_len = buffer_search(in_buf, sep, strlen(sep));

        if (header_len > 0)
        {
            recv_str.resize(header_len);
            buffer_remove(in_buf, (char *)recv_str.c_str(), header_len);
            state = HttpState::Http_Header_Parser;
        }
        else
        {
            break;
        }
    }

    case HttpState::Http_Header_Parser:
    {
        HttpRequestParser parser_;
        HttpRequestParser::ParseResult res = parser_.parse(req, (char *)recv_str.c_str(), (char *)recv_str.c_str() + recv_str.size());
        if (res == HttpRequestParser::ParsingError)
        {
            // 格式错误
            if (Global::Instance().get<int>("Debug") & 1)
            {
                std::cout << "Error ! header sytax error" << '\n';
            }
            state = HttpState::Http_Error;
            Close();
            return;
        }
        req.nv2map();
        if (Global::Instance().get<int>("Debug") & 1)
        {
            std::cout << req.inspect() << '\n';
        }
        // Content-Length
        std::string ctt = Global::Instance().get<std::string>("Content_length_type");
        if (req.HaveName(ctt))
        {
            state = HttpState::Http_Len_Parser;
            req.body_length = std::stoi(req.GetValue(ctt));
        }
        else
        {
            if (Global::Instance().get<int>("Debug") & 1)
            {
                std::cout << "info  nobody http request" << '\n';
            }
            state = HttpState::Http_Ready;
            goto http_ready;
        }
    }

    case HttpState::Http_Len_Parser:
    {
        // 如果缓冲区中已有足够 body 数据
        if (buffer_len(in_buf) >= req.body_length)
        {
            int header_len = recv_str.size();
            recv_str.resize(header_len + req.body_length);
            buffer_remove(in_buf, (char *)recv_str.c_str() + header_len, req.body_length);
            state = HttpState::Http_Body_Parser;
        }
        else
        {
            break;
        }
    }

    case HttpState::Http_Body_Parser:
    {
        HttpRequestParser parser_;
        req.clear();
        HttpRequestParser::ParseResult res = parser_.parse(req, (char *)recv_str.c_str(), (char *)recv_str.c_str() + recv_str.size());
        if (res == HttpRequestParser::ParsingError)
        {
            // 格式错误
            if (Global::Instance().get<int>("Debug") & 1)
            {
                std::cout << "Error ! Body Parser sytax error" << '\n';
            }
            state = HttpState::Http_Error;
            Close();
            return;
        }
        else
        {
            if (Global::Instance().get<int>("Debug") & 1)
            {
                std::cout << req.inspect() << '\n';
            }
            state = HttpState::Http_Ready;
        }
    }

    case HttpState::Http_Ready:
    {
    http_ready:
        // 如果是 HTTP/1.1 且支持 keep-alive，准备下一个请求
        state = HttpState::HttpCallback;
        if (req.keepAlive)
        {
            isKeepAlive = true;
        }
        DispatchHttpRequest(m_socket,req);
        break;
    }
    }
}


// 分发后,api使用connect请求
static int DispatchHttpRequest(int fd ,Request &req)
{
    //api_dispatch(fd,req.uri,req.Content2String());
    WorkPool::Instance().Submit( &api_dispatch,fd,req.uri,req.Content2String());
    return 0;
}