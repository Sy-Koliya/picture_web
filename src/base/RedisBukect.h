// RateLimiterRedisLua.h
#pragma once

#include "TimerEvent.h"
#include "RedisClient.h"
#include "ThrdPool.h"
#include <sw/redis++/redis++.h>
#include <coroutine>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <string>

// Lua 脚本：惰性刷新令牌桶并返回可授予的令牌数量
static constexpr char TOKEN_BUCKET_LUA[] = R"(
    local key       = KEYS[1]
    local now       = tonumber(ARGV[1])
    local rate      = tonumber(ARGV[2])
    local capacity  = tonumber(ARGV[3])
    local requested = tonumber(ARGV[4])
    
    -- 获取当前状态
    local data     = redis.call('HMGET', key, 'tokens', 'ts')
    local tokens   = tonumber(data[1]) or capacity
    local last_ts  = tonumber(data[2]) or now
    
    -- 计算新增令牌并合并
    local delta    = math.floor((now - last_ts) * rate)
    tokens       = math.min(capacity, tokens + delta)
    
    -- 计算要授予的令牌数
    local grant = math.min(tokens, requested)
    -- 只有当有令牌可授予时，才进行更新和通知
    if grant <= 0 then
        return 0
    end
    
    -- 更新桶状态
    tokens = tokens - grant
    redis.call('HMSET', key, 'tokens', tokens, 'ts', now)
    
    -- 发布“有新令牌”通知
    if tokens >0 then
    redis.call('PUBLISH', key .. ':chn', 'ok')
    end

    -- 返回实际授予数量，实现精确唤醒
    return grant
    )";

class RateLimiter
{
public:
    // 单例获取
    static RateLimiter &Instance()
    {
        static RateLimiter inst;
        return inst;
    }

    // 协程 Awaiter
    struct AcquireAwaiter
    {
        RateLimiter &parent;
        bool await_ready()
        {
            return parent.try_acquire(1) > 0;
        }
        void await_suspend(std::coroutine_handle<> h)
        {
            std::lock_guard lk(parent._mtx);
            parent._waiters.push_back(h);
        }
        void await_resume() {}
    };

    // 申请一个令牌
    AcquireAwaiter acquire() { return AcquireAwaiter{*this}; }

private:
    RateLimiter()
        : _redis(get_redis()), _running(true), _rate(100.0 / 1000.0), _cap(200)
    {
        // 加载 Lua 脚本
        _sha = _redis.script_load(TOKEN_BUCKET_LUA);

        // 订阅通知通道
        _sub = _redis.subscriber();
        _sub.on_message([this](std::string_view ch, std::string_view msg)
                        {
                    if (msg == "ok") on_token_published(); });
        _sub.subscribe(_bucket + ":chn");

        // 启动订阅线程
        _sub_thread = std::thread([this]()
                                  {
                    while (_running.load()) {
                        try { _sub.consume(); } catch(...) {}
                    } });

        // 周期检查等待队列，防止长时间挂起
        TimerEventManager::Instance().Create(
            [this]()
            { this->periodic_check(); },
            100 /*ms*/, -1);
    }

    ~RateLimiter()
    {
        _running.store(false);
        if (_sub)
            _sub.unsubscribe(_bucket + ":chn");
        if (_sub_thread.joinable())
            _sub_thread.join();
    }

    // 从 Redis 脚本获取授予的数量
    size_t try_acquire(size_t count)
    {
        auto now = ms_now();
        try
        {
            long granted = _redis.evalsha<long>(
                _sha,
                {_bucket},
                {std::to_string(now),
                 std::to_string(_rate),
                 std::to_string(_cap),
                 std::to_string(count)});
            return granted;
        }
        catch (...)
        {
            return 0;
        }
    }

    // 发布通知时批量 resume 授予的协程
    void on_token_published()
    {
        std::deque<std::coroutine_handle<>> waiters;
        {
            std::lock_guard lk(_mtx);
            waiters.swap(_waiters);
        }
        size_t n = waiters.size();
        size_t granted = try_acquire(n);

        // resume granted 个协程
        for (size_t i = 0; i < granted && i < waiters.size(); ++i)
            WorkPool::Instance().Submit([](){ waiters[i].resume();});

        // 未授予的重新入队
        std::lock_guard lk(_mtx);
        for (size_t i = granted; i < waiters.size(); ++i)
            _waiters.push_back(waiters[i]);
    }

    // 周期检查，尝试 resume 等待的协程
    void periodic_check()
    {
        std::deque<std::coroutine_handle<>> waiters;
        {
            std::lock_guard lk(_mtx);
            if (_waiters.empty())
                return;
            waiters.swap(_waiters);
        }
        size_t n = waiters.size();
        size_t granted = try_acquire(n);
        for (size_t i = 0; i < granted; ++i)
        WorkPool::Instance().Submit([](){ waiters[i].resume();});
        std::lock_guard lk(_mtx);
        for (size_t i = granted; i < waiters.size(); ++i)
            _waiters.push_back(waiters[i]);
    }

    static long ms_now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    sw::redis::Redis &_redis;
    sw::redis::Subscriber _sub;
    std::thread _sub_thread;
    std::atomic<bool> _running;
    std::string _bucket = "_tuchuang_bucket";
    std::string _sha;
    double _rate;
    size_t _cap;
    std::mutex _mtx;
    std::deque<std::coroutine_handle<>> _waiters;
};

// 使用示例：
// RateLimiter limiter("tcp://127.0.0.1:6379", "mybucket", 100.0, 200);
//
// RpcTask<void> handler() {
//   co_await limiter.acquire();
//   // 处理逻辑
// }
