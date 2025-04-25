// GlobalVars.h
#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <string>
#include <unordered_map>
#include <any>           
#include <mutex>
#include <stdexcept>
#include "types.h"



class Global {
public:
    // 获取单例
    static Global& Instance() {
        static Global inst;
        return inst;
    }

    // 设置全局变量，若已存在则覆盖
    template<typename T>
    void set(const std::string& key, T value) {
        std::lock_guard<std::mutex> lk(mutex_);
        vars_[key] = std::move(value);
    }

    // 获取全局变量，若不存在或类型不匹配则抛异常
    template<typename T>
    T get(const std::string& key) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = vars_.find(key);
        if (it == vars_.end()) {
            throw std::runtime_error("GlobalVars::get: key not found: " + key);
        }
        try {
            return std::any_cast<T>(it->second);
        }
        catch (const std::bad_any_cast&) {
            throw std::runtime_error("GlobalVars::get: bad cast for key: " + key);
        }
    }

    // 判断是否存在某个 key
    bool contains(const std::string& key) const {
        std::lock_guard<std::mutex> lk(mutex_);
        return vars_.count(key) != 0;
    }

    // 删除某个 key
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lk(mutex_);
        vars_.erase(key);
    }

private:
    Global() ;
    ~Global() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> vars_;
};

#endif // GLOBAL_VARS_H
