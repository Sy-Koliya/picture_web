#ifndef RPCCOROUTINE_HPP
#define RPCCOROUTINE_HPP

#include "RpcCoroutine.h"
#include "CoroutineCompeleteQueue.h"
// 一字不漏：Coroutine_finish 定义
template <typename T>
void Coroutine_finish(Notify<T>* nt) {
    CoroutineScheduler::Instance().finish(nt);
}

#endif // RPCCOROUTINE_HPP
