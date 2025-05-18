#ifndef RPCCOROUTINE_HPP
#define RPCCOROUTINE_HPP

#include "RpcCoroutine.h"
#include "CoroutineCompeleteQueue.h"
void Coroutine_finish(int nid) {
    CoroutineScheduler::Instance().finish(nid);
}

#endif // RPCCOROUTINE_HPP
