#ifndef RPCCOROUTINE_HPP
#define RPCCOROUTINE_HPP

#include "RpcCoroutine.h"
#include "CoroutineCompeleteQueue.h"
template<typename T>
void Coroutine_finish(std::shared_ptr<Notify<T>> nt) {
    CoroutineScheduler::Instance().finish(nt);
}

#endif // RPCCOROUTINE_HPP
