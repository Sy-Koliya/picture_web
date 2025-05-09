#include "HttpRequestParser.h"
#include "HttpRequest.h"
#include "buffer.h"
#include <iostream>
#include <string>
using namespace httpparser;

int main()
{

    const char text[] =
                        "GET /uri.cgi HTTP/1.1\r\n"
                        "User-Agent: Mozilla/5.0\r\n"
                        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                        "Host: 127.0.0.1\r\n"
                        "\r\n";

    buffer_t *buf = buffer_new(1);
    buffer_add(buf, text, strlen(text));
    const char *sep = "\r\n\r\n";
    int len = buffer_search(buf, sep, strlen(sep));
    if (len == 0)
    {
        std::cout << "error ! cannot find \r\n\r\n"
                  << '\n';
    }
    else
    {
        Request request;
        HttpRequestParser parser;
        char *body = (char *)malloc(len);
        buffer_remove(buf, body, len);
        std::cout << body << '\n';
        HttpRequestParser::ParseResult res = parser.parse(request, body, body + len);
        if (res == HttpRequestParser::ParsingCompleted)
        {
            std::cout << request.inspect() << std::endl;
            return EXIT_SUCCESS;
        }
        else
        {
            std::cerr << "Parsing failed" << std::endl;
            return EXIT_FAILURE;
        }
        delete body;
    }
}