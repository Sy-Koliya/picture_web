#include "HttpConn.h"
#include "HttpRequestParser.h"
#include "Global.h"
#include <string.h>

using namespace httpparser;

HttpConn::HttpConn()
    : BaseSocket(),
      state(HttpState::Http_Header_Read),
      last_recv(std::chrono::steady_clock::now())
{
    // any other setup…
}

// Destructor
HttpConn::~HttpConn() = default;

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
    if (Global::Instance().get<int>("Debug") & 1)
    {
        std::cout << "recv new msg:\n   " << buff << '\n';
    }
    buffer_add(in_buf, buff, len);
    if (buffer_len(in_buf) >= max_recv_len)
    {
        // 可能被攻击，进一步处理
    }
    HandleRead();
    return 0;
}
int HttpConn::Write_imp()
{
    return 0;
}
int HttpConn::Connect_imp()
{
    return 0;
}

int HttpConn::Close_imp()
{
    // 如果发生错误要告诉客户端
    if (state == HttpState::Http_Error)
    {
        // HttpManager::Instacn().create_404_responce(m_remote_ip.c_str,m_remote_port);
    }
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
        if (Global::Instance().get<int>("Debug") & 1){
            std::cout<<"state :Http_Header_Read "<<'\n';
        }
        int header_len = buffer_search(in_buf, sep, strlen(sep));

        if (header_len>0)
        {
            if (Global::Instance().get<int>("Debug") & 1)
                std::cout << "find \r\n\r\n"
                          << '\n';
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
        if (!Global::Instance().contains(req.uri))
        {
            if (Global::Instance().get<int>("Debug") & 1)
            {
                std::cout << "Error ! uri not found " << '\n';
            }
            state = HttpState::Http_Error;
            Close();
            return;
        }
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
                std::cout << "Error ! " << Global::Instance().get<std::string>("Content_length_type") << "  not found " << '\n';
            }
            state = HttpState::Http_Error;
            Close();
            return;
        }
        //  chunked 编码
        // std::string chk =  Global::Instance().get<std::string>("Chunked_type");
        //  if(req.HaveName(chk)){
        //     state_ |=HttpState::Http_Chunked_Parser;
        //  }
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
        state = HttpState::HttpCallback;
    }

    case HttpState::HttpCallback:
    {
        // 如果是 HTTP/1.1 且支持 keep-alive，准备下一个请求
        if (req.keepAlive)
        {
            if (Global::Instance().get<int>("Debug") & 1)
            {
                Send((char *)req.Content2String().c_str(), req.body_length);
            }
            state = HttpState::Http_Header_Read;
            // 交给上层 API 分发器处理
            // dispatchHttpRequest(std::move(req));
        }
        else
        {
            if (Global::Instance().get<int>("Debug") & 1)
            {
                Send((char *)req.Content2String().c_str(), req.body_length);
            }
            // 短连接，处理完就关闭
            // dispatchHttpRequest(std::move(req));
            Close();
        }
        break;
    }
    }
    // add_alive_set (this)
}

// 分发后,api使用connect请求
static void DispatchHttpRequest(Request req)
{
    // std::string uri = req.uri;
    // using call = Global::Instance().get< callback >(uri);
    // auto handle = call(req);
}