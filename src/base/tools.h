#ifndef TOOLS_H
#define TOOLS_H

/*
工具库头文件
注意普通函数需要inline展开避免重定义

*/

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <chrono>
#include <memory>
#include <mutex>
#include "types.h"

// wrap_arg_: 左值引用使用 std::ref，右值按值传递
template <typename T>
decltype(auto) wrap_arg_(T &&arg)
{
    if constexpr (std::is_lvalue_reference_v<T>)
    {
        return std::ref(arg);
    }
    else
    {
        return std::forward<T>(arg);
    }
}

// Package2FVV: 将任意可调用对象和参数封装为 std::function<void()>
template <typename Func, typename... Args>
std::function<void()> Package2FVV(Func &&f, Args &&...args)
{
    // 把函数和每个参数都包进一个 tuple
    auto tup = std::make_tuple(
        wrap_arg_(std::forward<Args>(args))...);

    // 构造可拷贝的 lambda，并转换为 std::function<void()>
    return std::function<void()>(
        [func = std::forward<Func>(f),
         tup = std::move(tup)]() mutable
        {
            std::apply(func, tup);
        });
}

class IDhelper {
public:
    IDhelper() : next_id(0) {}

    int Get() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!free_ids_.empty()) {
            int id = free_ids_.back();
            free_ids_.pop_back();
            return id;
        }
        return ++next_id;
    }


    void Del(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id > next_id)
            return;
        if (id == next_id) {

            --next_id;
        } 
        free_ids_.push_back(id);
    }

private:
    std::mutex             mutex_;
    std::vector<int>       free_ids_;
    int                    next_id;
};

inline uint64_t get_tick_count()
{
    // 1. 获取当前 steady_clock 时间点
    auto tp = std::chrono::steady_clock::now();

    // 2. 转为自 epoch 以来的纳秒数
    auto ns = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch())
                  .count();

    return static_cast<uint64_t>(ns);
}

inline size_t align_pow_2(size_t n)
{
    if (n == 0)
        return 0;
    std::size_t p = 1;
    while ((p << 1) <= n)
    {
        p <<= 1;
    }
    return p;
}





// void writePid()
// {
//     uint32_t curPid;
//     curPid = (uint32_t)getpid();
//     FILE *f = fopen("server.pid", "w");
//     assert(f);
//     char szPid[32];
//     snprintf(szPid, sizeof(szPid), "%d", curPid);
//     fwrite(szPid, strlen(szPid), 1, f);
//     fclose(f);
// }

#endif
