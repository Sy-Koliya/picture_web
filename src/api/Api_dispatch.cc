#include "Api_dispatch.h"
#include "HttpConn.h"

#include <string>
#include <map>

template <typename T>
class RpcTask;

using Handler = RpcTask<int> (*)(int, const std::string &, const std::string &);

struct TrieNode
{
    std::map<char, TrieNode *> children;
    Handler func = nullptr; // 默认空指针
    ~TrieNode()
    {
        for (auto &p : children)
            delete p.second;
    }
};

class UriTrie
{
public:
    UriTrie() : root_(new TrieNode) {}
    ~UriTrie() { delete root_; }

    // 插入一条路由
    void insert(const std::string &uri, Handler f)
    {
        TrieNode *cur = root_;
        for (char c : uri)
        {
            if (!cur->children.count(c))
                cur->children[c] = new TrieNode;
            cur = cur->children[c];
        }
        cur->func = f;
    }

    // 找最长前缀匹配
    Handler find(const std::string &path) const
    {
        TrieNode *cur = root_;
        Handler best = nullptr;
        for (char c : path)
        {
            auto it = cur->children.find(c);
            if (it == cur->children.end())
                break;
            cur = it->second;
            if (cur->func)
                best = cur->func;
        }
        return best;
    }

    UriTrie(const UriTrie &) = delete;
    UriTrie &operator=(const UriTrie &) = delete;

private:
    TrieNode *root_;
};

class UriDispatcher
{
private:
    static UriTrie &getTrie()
    {
        static UriTrie instance; // 线程安全的单例
        return instance;
    }

    static std::once_flag &getFlag()
    {
        static std::once_flag flag; // 注册标记
        return flag;
    }

public:
    // 路由注册入口（线程安全）
    static void Register(const std::string &uri = "", Handler handle_ = nullptr)
    {
        std::call_once(getFlag(), [&]()
                       {
                           // 基础路由示例
                           getTrie().insert("/api/reg", ApiRegisterUser);
                           getTrie().insert("/api/md5", ApiInstantUpload);
                           getTrie().insert("/api/login", ApiUserLogin);
                           getTrie().insert("/api/upload",ApiUploadFile);
                           getTrie().insert("/api/myfiles",ApiMyfiles);
                       });
        // 动态添加:
    }
    static auto FindHandler(const std::string &uri)
    {
        return getTrie().find(uri);
    }
};

void api_dispatch(int fd,
                  const std::string &uri,
                  const std::string &content)
{

    static bool _ = []()
    {
        UriDispatcher::Register();
        return true;
    }();

    try
    {

        BaseCount sock = FindBaseSocket(fd);
        if(!sock)return ;
        auto task = UriDispatcher::FindHandler(uri);
        if (task == nullptr)
        {
                if (auto *h = dynamic_cast<HttpConn *>(sock.GetBasePtr()))
                {
                    h->SetErrorResponse(500,"url not found");
                }
            return;
        }
        coro_register<int>(std::move(task(fd, content, uri)),
                           [fd,sock](int code)
                           {
                           });
    }
    catch (const std::exception &e)
    {
        std::cerr << "Dispatch error: " << e.what() << std::endl;
    }
}
