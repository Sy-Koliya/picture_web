#ifndef DISPATCHAPI_H
#define DISPATCHAPI_H
#include <string>
#include <mutex>
#include "api_login.h"
#include "api_register.h"
#include "api_instant_upload.h"
#include "api_upload.h"
#include "Global.h"
#include "CoroutineCompeleteQueue.h"



void api_dispatch(int fd,
                         const std::string &uri,
                         const std::string &content);

#endif