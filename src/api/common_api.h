#ifndef API_COMMOM_H
#define API_COMMOM_H
#include <string>
#include "RedisClient.h"

#define FILE_NAME_LEN (256)    // 文件名字长度
#define TEMP_BUF_MAX_LEN (512) // 临时缓冲区大小
#define FILE_URL_LEN (512)     // 文件所存放storage的host_name长度
#define HOST_NAME_LEN (30)     // 主机ip地址长度
#define USER_NAME_LEN (128)    // 用户名字长度
#define TOKEN_LEN (128)        // 登陆token长度
#define MD5_LEN (256)          // 文件md5长度
#define PWD_LEN (256)          // 密码长度
#define TIME_STRING_LEN (25)   // 时间戳长度
#define SUFFIX_LEN (8)         // 后缀名长度
#define PIC_NAME_LEN (10)      // 图片资源名字长度
#define PIC_URL_LEN (256)      // 图片资源url名字长度

#define HTTP_RESP_OK 0
#define HTTP_RESP_FAIL 1           //
#define HTTP_RESP_USER_EXIST 2     // 用户存在
#define HTTP_RESP_DEALFILE_EXIST 3 // 别人已经分享此文件
#define HTTP_RESP_TOKEN_ERR 4      //  token验证失败
#define HTTP_RESP_FILE_EXIST 5     // 个人已经存储了该文件

std::string RandomString(std::size_t len);

bool VerifyToken(const std::string &user_name, const std::string &token);


bool QueryParseKeyValue(const std::string &query,
                        const std::string &key,
                        std::string &value);







#endif
