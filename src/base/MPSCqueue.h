
#ifndef _MARK_MPSC_QUEUE_H
#define _MARK_MPSC_QUEUE_H

#include <atomic>
#include <memory>
#include <utility>



// tail -> node -> ... ->head(哨兵节点)->dummy
template<typename T>
class MPSCQueueNonIntrusive {
public:
    MPSCQueueNonIntrusive() : _head(new Node()), _tail(_head.load(std::memory_order_relaxed)) {
        Node* front = _head.load(std::memory_order_relaxed);
        front->Next.store(nullptr, std::memory_order_relaxed);
    }

    ~MPSCQueueNonIntrusive() {
        std::shared_ptr<T> output;
        while (Dequeue(output)) {} // 自动释放 shared_ptr
        Node* front = _head.load(std::memory_order_relaxed);
        delete front;
    }

    void Enqueue(std::shared_ptr<T> input) {
        Node* node = new Node(std::move(input));
        Node* prevHead = _head.exchange(node, std::memory_order_acq_rel);
        prevHead->Next.store(node, std::memory_order_release);
    }

    bool Dequeue(std::shared_ptr<T>& result) {
        Node* tail = _tail.load(std::memory_order_relaxed);
        Node* next = tail->Next.load(std::memory_order_acquire);
        if (!next) return false;

        result = std::move(next->Data);
        _tail.store(next, std::memory_order_release);
        delete tail;
        return true;
    }

    bool empty() const noexcept {
        Node* tail = _tail.load(std::memory_order_acquire);
        return tail->Next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        std::shared_ptr<T> Data;
        std::atomic<Node*> Next;

        explicit Node(std::shared_ptr<T> data = nullptr) 
            : Data(std::move(data)), Next(nullptr) {}
    };

    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;
};
template<typename T>
using MPSCQueue = MPSCQueueNonIntrusive<T>;

#endif // MPSCQueue_h__


