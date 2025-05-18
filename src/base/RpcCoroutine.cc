#include "RpcCoroutine.h"
#include "CoroutineCompeleteQueue.h"

void coro_finish(void* key) {
    CoroutineScheduler::Instance().finish(key);
}