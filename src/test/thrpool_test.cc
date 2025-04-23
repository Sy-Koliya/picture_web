#include "ThrdPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
// 普通函数示例
void print_number(int num) {
    std::cout << "Number: " << num << std::endl;
}

// 函数对象示例
struct Adder {
    int operator()(int a, int b) const {
        std::cout << a << " + " << b << " = " << (a + b) << std::endl;
        return a + b;
    }
};

// 类成员函数示例
class TaskProcessor {
public:
    void process(const std::string& message, int priority) {
        std::cout << "Processing [" << message << "] with priority " << priority << std::endl;
    }
};

int main() {
    try {
        // 创建4个线程的线程池
        ThreadPool pool(4);

        // 提交lambda表达式
        pool.Submit([](int a, int b) {
            //std::this_thread::sleep_for(std::chrono::seconds(4));
            std::cout << "Lambda result: " << a * b << std::endl;
        }, 5, 6);

        // 提交普通函数
        for (int i = 0; i < 3; ++i) {
            pool.Submit(print_number, i+1);
        }

        // 提交函数对象
        Adder adder;
        pool.Submit(adder, 10, 20);
        pool.Submit(Adder{}, 30, 40);

        // 提交类成员函数
        TaskProcessor processor;
        pool.Submit(&TaskProcessor::process, &processor, "Important task", 9);

        // 提交复杂任务
        std::vector<int> data{1, 2, 3, 4, 5};
        pool.Submit([data](double factor) {
            double sum = 0;
            for (int n : data) {
                sum += n * factor;
            }
            std::cout << "Weighted sum: " << sum << std::endl;
        }, 2.5);

        // 显示当前线程数
        std::cout << "Current thread count: " << pool.ThreadCount() << std::endl;

        // 等待任务完成
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}