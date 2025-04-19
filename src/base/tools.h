#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>


//ref 并不保证引用的生命周期，可能会出现悬挂现象，请保证资源的生命周期
template <typename T>
 decltype(auto) wrap_arg_(T &&arg) {
    if constexpr (std::is_lvalue_reference<T>::value) {
        return std::ref(arg);
    } else {
        return std::forward<T>(arg);
    }
}

//Package2FVV: 打包成 std::function<void()>
 template <typename Func, typename... Args>
std::function<void()> Package2FVV(Func&& f, Args&&... args) {
    // 把函数和每个参数都包进一个 tuple
    auto tup = std::make_tuple(
        wrap_arg_(std::forward<Args>(args))...
    );

    // 构造可拷贝的 lambda，并转换为 std::function<void()>
    return std::function<void()>(
        [ func = std::forward<Func>(f),
          tup  = std::move(tup) ]() mutable
        {
            std::apply(func, tup);
        }
    );
}


//no threads empty
class IDhelper{
public:
    int get(){}
    void del(int id){}
private:
    std::vector<int>stk;
    int idx;
};


// void writePid()
// {
//     uint32_t curPid;
// #ifdef _WIN32
//     curPid = (uint32_t)GetCurrentProcess();
// #else
//     curPid = (uint32_t)getpid();
// #endif
//     FILE *f = fopen("server.pid", "w");
//     assert(f);
//     char szPid[32];
//     snprintf(szPid, sizeof(szPid), "%d", curPid);
//     fwrite(szPid, strlen(szPid), 1, f);
//     fclose(f);
// }

