// GlobalVars.h
#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <string>
#include <unordered_map>
#include <any>
#include <mutex>
#include <stdexcept>
#include "types.h"

#include <functional>
//#include "RpcCoroutine.h" // 假设 RpcTask 定义在此头文件中


class IRpcHandler {
    public:
        virtual ~IRpcHandler() = default;
        virtual std::any Invoke(int fd, const std::vector<std::any>& args) = 0;
};

template<typename Ret, typename... Args>
class RpcHandlerImpl : public IRpcHandler {
public:
    using HandlerFunc = std::function<Ret(int, Args...)>;

    explicit RpcHandlerImpl(HandlerFunc func) : func_(std::move(func)) {}

    std::any Invoke(int fd, const std::vector<std::any>& args) override {
        // 检查参数数量是否匹配
        if (args.size() != sizeof...(Args)) {
            throw std::runtime_error("Argument count mismatch");
        }

        // 将 any 参数转换为具体类型
        return InvokeImpl(fd, args, std::index_sequence_for<Args...>{});
    }

private:
    template<size_t... Is>
    std::any InvokeImpl(int fd,  const std::vector<std::any>& args, std::index_sequence<Is...>) {
        // 类型安全转换（失败时抛 bad_any_cast）
        return func_(fd, std::any_cast<Args>(args[Is])...);
    }

    HandlerFunc func_;
};
class Global
{
public:
    // 获取单例
    static Global &Instance()
    {
        static Global inst;
        return inst;
    }

    // 设置全局变量，若已存在则覆盖
    template <typename T>
    void set(const std::string &key, T value)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        vars_[key] = std::move(value);
    }

    // 获取全局变量，若不存在或类型不匹配则抛异常
    template <typename T>
    T get(const std::string &key) const
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = vars_.find(key);
        if (it == vars_.end())
        {
            throw std::runtime_error("GlobalVars::get: key not found: " + key);
        }
        try
        {
            return std::any_cast<T>(it->second);
        }
        catch (const std::bad_any_cast &)
        {
            throw std::runtime_error("GlobalVars::get: bad cast for key: " + key);
        }
    }

    // 判断是否存在某个 key
    bool contains(const std::string &key) const
    {
        std::lock_guard<std::mutex> lk(mutex_);
        return vars_.count(key) != 0;
    }

    // 删除某个 key
    void remove(const std::string &key)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        vars_.erase(key);
    }

    /*

    // 注册到 Global
    Global::Instance().RegisterUriHandler<RpcTask<int>, std::string&>(
        "/api/register", 
        &ApiRegisterUser
    );

    Global::Instance().RegisterUriHandler<RpcTask<std::string>>(
        "/api/info",
        [](int fd, uint32_t conn_uuid) { return GetServerInfo(fd, conn_uuid); }
    );
    // 处理请求的通用逻辑
    void HandleRequest(const std::string& uri, int fd, uint32_t conn_uuid) {
        try {
            if (uri == "/api/register") {
                std::string post_data = ReadPostData();
                auto task = Global::Instance().CallUriHandler<RpcTask<int>>(
                    uri, fd, conn_uuid, {post_data}
                );
                // 处理 task...
            } else if (uri == "/api/info") {
                auto task = Global::Instance().CallUriHandler<RpcTask<std::string>>(
                    uri, fd, conn_uuid, {}
                );
                // 处理 task...
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    */
   
    // 注册 URI 处理函数（模板方法）
    template<typename Ret, typename... Args>
    void RegisterUriHandler(const std::string& uri, std::function<Ret(int,Args...)> handler) {
        std::lock_guard<std::mutex> lk(mutex_);
        uri_handlers_[uri] = std::make_unique<RpcHandlerImpl<Ret, Args...>>(handler);
    }

    // 调用 URI 处理函数
    template<typename Ret>
    Ret CallUriHandler(const std::string& uri, int fd,  const std::vector<std::any>& args) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = uri_handlers_.find(uri);
        if (it == uri_handlers_.end()) {
            throw std::runtime_error("URI handler not found: " + uri);
        }

        std::any result = it->second->Invoke(fd, args);
        return std::any_cast<Ret>(result);
    }

    // 调用前检查 URI 是否存在
    bool HasUriHandler(const std::string& uri) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return uri_handlers_.find(uri) != uri_handlers_.end();
    }


private:
    std::unordered_map<std::string, std::unique_ptr<IRpcHandler>> uri_handlers_;

private:
    Global();
    ~Global() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> vars_;
};



#endif // GLOBAL_VARS_H
