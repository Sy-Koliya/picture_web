#ifndef DISPATCHAPI_H
#define DISPATCHAPI_H
#include <string>
#include <mutex>
#include "Global.h"
#include "api_login.h"
#include "api_register.h"
#include "api_instant_upload.h"
#include "api_upload.h"
#include "api_myfiles.h"
#include "api_dealfile.h"
#include "api_dealsharedfile.h"
#include "api_sharefiles.h"
#include "CoroutineCompeleteQueue.h"

void api_dispatch(int fd,
                         const std::string &uri,
                         const std::string &content);

#endif