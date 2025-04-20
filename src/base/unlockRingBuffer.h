#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define RINGBUFFER_SIZE 1024  // 必须是2的幂

typedef struct {
    int buffer[RINGBUFFER_SIZE];
    atomic_size_t head;  // 写入位置
    atomic_size_t tail;  // 读取位置
} RingBuffer;

// 初始化环形缓冲区
RingBuffer* ringbuffer_create() {
    RingBuffer* rb = (RingBuffer*)malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    
    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);
    memset(rb->buffer, 0, sizeof(rb->buffer));
    return rb;
}

// 销毁环形缓冲区
void ringbuffer_destroy(RingBuffer* rb) {
    free(rb);
}

// 尝试将数据推入缓冲区（生产者线程安全）
bool ringbuffer_push(RingBuffer* rb, int data) {
    size_t current_head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t current_tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    
    // 检查缓冲区是否已满（使用掩码优化计算）
    if ((current_head - current_tail) >= RINGBUFFER_SIZE) {
        return false;
    }

    // 写入数据
    rb->buffer[current_head & (RINGBUFFER_SIZE - 1)] = data;

    // 使用原子操作更新head指针（memory_order_release保证写入可见性）
    atomic_store_explicit(&rb->head, current_head + 1, memory_order_release);
    return true;
}

// 尝试从缓冲区弹出数据（消费者线程安全）
bool ringbuffer_pop(RingBuffer* rb, int* out_data) {
    size_t current_head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t current_tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    // 检查缓冲区是否为空
    if (current_tail == current_head) {
        return false;
    }

    // 读取数据
    *out_data = rb->buffer[current_tail & (RINGBUFFER_SIZE - 1)];

    // 使用原子操作更新tail指针
    atomic_store_explicit(&rb->tail, current_tail + 1, memory_order_release);
    return true;
}

/********************** 测试代码 ************************/
// #define TEST_ITERATIONS 1000000L

// // 生产者线程函数
// int producer_thread(void* arg) {
//     RingBuffer* rb = arg;
//     for (int i = 1; i <= TEST_ITERATIONS; ++i) {
//         while (!ringbuffer_push(rb, i)) {
//             thrd_yield();  // 缓冲区满时让出CPU
//         }
//     }
//     return 0;
// }

// // 消费者线程函数
// int consumer_thread(void* arg) {
//     RingBuffer* rb = arg;
//     long sum = 0;
//     int value;
//     for (long i = 0; i < TEST_ITERATIONS; ) {
//         if (ringbuffer_pop(rb, &value)) {
//             sum += value;
//             ++i;
//         } else {
//             thrd_yield();
//         }
//     }
//     printf("验证结果: 期待值=%ld, 实际值=%ld\n", 
//         (TEST_ITERATIONS * (TEST_ITERATIONS + 1)) / 2, sum);
//     return 0;
// }

// int main() {
//     RingBuffer* rb = ringbuffer_create();
//     if (!rb) {
//         fprintf(stderr, "创建环形缓冲区失败\n");
//         return EXIT_FAILURE;
//     }

//     thrd_t producer, consumer;
//     thrd_create(&producer, producer_thread, rb);
//     thrd_create(&consumer, consumer_thread, rb);

//     thrd_join(producer, NULL);
//     thrd_join(consumer, NULL);

//     ringbuffer_destroy(rb);
//     return EXIT_SUCCESS;
// }