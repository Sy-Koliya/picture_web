#include "tools.h"
// wrap_arg_: 对于左值保留引用，否则完美转发

class IDhelper
{
public:
    IDhelper()
    {
        idx = 0;
    }
    ~IDhelper() {}
    int get()
    {
        if (!stk.empty())
        {
            int tmp = stk.back();
            stk.pop_back();
             return tmp;
        }
        return ++idx;
    }
    void del(int id)
    {
        if (id > idx)
            return;
        if (id == idx)
            idx--;
        stk.push_back(id);
    }

private:
    std::vector<int> stk;
    int idx;
};
