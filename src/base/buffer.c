#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/uio.h>
#include <pthread.h>
#include <unistd.h>
#include "buffer.h"



//伪线程安全，后续可能会更新

// 链节点结构体：用于存储一段数据
struct buf_chain_s
{
    struct buf_chain_s *next; // 指向下一个链节点
    uint32_t buffer_len;      // 本节点分配的缓冲区总长度
    uint32_t misalign;        // 数据起始位置偏移（已读/删除的数据区间）
    uint32_t off;             // 本节点中有效数据长度
    uint8_t *buffer;          // 指向实际存放数据的内存
};

// 缓冲区结构体：由多个链节点组成的环状链表
struct buffer_s
{
    buf_chain_t *first;            // 链表头
    buf_chain_t *last;             // 链表尾，便于快速追加
    buf_chain_t **last_with_datap; // 指向最近一次写入数据的节点指针引用，优化插入和清理
    uint32_t total_len;            // 缓冲区中所有链节点的有效数据总长度
    uint32_t last_read_pos;        // 上次搜索时匹配停留的全局偏移
    pthread_mutex_t lock;
};

// 计算链节点可写剩余空间
#define CHAIN_SPACE_LEN(ch) ((ch)->buffer_len - ((ch)->misalign + (ch)->off))
#define MIN_BUFFER_SIZE 1024                                   // 链节点最小分配尺寸
#define MAX_TO_COPY_IN_EXPAND 4096                             // 扩容时一次最多复制的数据量
#define BUFFER_CHAIN_MAX_AUTO_SIZE 4096                        // 自动扩容阈值
#define MAX_TO_REALIGN_IN_EXPAND 2048                          // 最大重排阈值
#define BUFFER_CHAIN_MAX (16 * 1024 * 1024)                    // 单链节点最大容量 16MB
#define BUFFER_CHAIN_EXTRA(t, c) (t *)((buf_chain_t *)(c) + 1) // 计算链节点数据区起始地址
#define BUFFER_CHAIN_SIZE sizeof(buf_chain_t)                  // 链节点头结构大小
#define REQURIEDLEN_WITH_KMP 8                                 // 小于此长度使用暴力搜索，以上使用 KMP
#define IOV_NUMS 4                                             // readv调用的iov的默认个数
/**
 * ZERO_CHAIN: 重置 buffer 结构
 * 清空链表相关指针和统计信息
 */
static inline void
ZERO_CHAIN(buffer_t *dst)
{
    dst->first = NULL;
    dst->last = NULL;
    dst->last_with_datap = &dst->first;
    dst->total_len = 0;
    dst->last_read_pos = 0;
}

/**
 * buffer_len: 获取当前缓冲区有效数据总长度
 */
uint32_t
buffer_len(buffer_t *buf)
{
    pthread_mutex_lock(&buf->lock);
    uint32_t len = buf->total_len;
    pthread_mutex_unlock(&buf->lock);
    return len;
}

/**
 * buffer_new: 创建并初始化 buffer 对象
 * sz 参数暂不使用，可用于预留容量扩展
 */
buffer_t *
buffer_new(uint32_t sz)
{
    (void)sz;
    buffer_t *buf = malloc(sizeof(*buf));
    if (!buf) {
        return NULL;
    }
    // 清 0 所有字段
    memset(buf, 0, sizeof(*buf));

    // 必要的初始值
    buf->first = NULL;
    buf->last  = NULL;
    buf->total_len = 0;
    buf->last_with_datap = &buf->first;
    pthread_mutex_init(&buf->lock, NULL);

    return buf;
}

/**
 * buf_chain_new: 分配并初始化一个链节点
 * size: 期望存储的数据长度
 * 返回新节点指针或 NULL
 */
static buf_chain_t *
buf_chain_new(uint32_t size)
{
    buf_chain_t *chain;
    uint32_t to_alloc;
    if (size > BUFFER_CHAIN_MAX - BUFFER_CHAIN_SIZE)
        return NULL;
    size += BUFFER_CHAIN_SIZE;
    // 取 2 的幂次增长，至少 MIN_BUFFER_SIZE
    if (size < BUFFER_CHAIN_MAX / 2)
    {
        to_alloc = MIN_BUFFER_SIZE;
        while (to_alloc < size)
        {
            to_alloc <<= 1;
        }
    }
    else
    {
        to_alloc = size;
    }
    chain = (buf_chain_t *)malloc(to_alloc);
    if (!chain)
        return NULL;
    memset(chain, 0, BUFFER_CHAIN_SIZE);
    chain->buffer_len = to_alloc - BUFFER_CHAIN_SIZE;
    chain->buffer = BUFFER_CHAIN_EXTRA(uint8_t, chain);
    return chain;
}

/**
 * buf_chain_free_all: 释放从 chain 开始的所有节点
 */
static void
buf_chain_free_all(buf_chain_t *chain)
{
    buf_chain_t *next;
    while (chain)
    {
        next = chain->next;
        free(chain);
        chain = next;
    }
}

/**
 * buffer_free: 释放 buffer 及其中所有链节点
 */
void buffer_free(buffer_t *buf)
{
    if (buf)
    {
        buf_chain_free_all(buf->first);
        ZERO_CHAIN(buf);
        pthread_mutex_destroy(&buf->lock);
        free(buf);
    }
}

/**
 * free_empty_chains: 清理 last_with_datap 之后的空链节点
 * 返回清理后可插入的位置引用
 */
static buf_chain_t **
free_empty_chains(buffer_t *buf)
{
    buf_chain_t **ch = buf->last_with_datap;
    while ((*ch) && (*ch)->off != 0)
        ch = &(*ch)->next;
    if (*ch)
    {
        buf_chain_free_all(*ch);
        *ch = NULL;
    }
    return ch;
}

/**
 * buf_chain_insert: 将节点插入 buffer 链中，并更新统计
 */
static 
void buf_chain_insert(buffer_t *buf, buf_chain_t *chain)
{
    if (chain == NULL) return;

    // 如果链表目前是空的
    if (*buf->last_with_datap == NULL) {
        buf->first = chain;
    } else {
        // 把 chain 接到上一个有数据链的 next 上
        (*buf->last_with_datap)->next = chain;
    }

    buf->last = chain;
    // 下一次插入，还应该挂在这个 chain->next 上
    buf->last_with_datap = &chain->next;
}


static int
buf_chain_should_realign(buf_chain_t *chain, uint32_t datlen)
{
    return chain->buffer_len - chain->off >= datlen &&
           (chain->off < chain->buffer_len / 2) &&
           (chain->off <= MAX_TO_REALIGN_IN_EXPAND);
}

static void
buf_chain_align(buf_chain_t *chain)
{
    memmove(chain->buffer, chain->buffer + chain->misalign, chain->off);
    chain->misalign = 0;
}

/**
 * buffer_add: 向 buffer 追加数据
 * 1) 尝试写入 last_with_datap 所指节点
 * 2) 若空间不足，判断是否可重排或扩容
 * 3) 最终新建节点并插入
 */
int buffer_add(buffer_t *buf, const void *data_in, uint32_t datlen)
{
    pthread_mutex_lock(&buf->lock);
    buf_chain_t *chain, *tmp;
    const char *data = (char *)data_in;
    uint32_t remain, to_alloc;
    if (datlen > BUFFER_CHAIN_MAX - buf->total_len)
    {
    pthread_mutex_unlock(&buf->lock);
        return -1; // 超出最大容量
    }
    // 定位写入节点
    chain = *buf->last_with_datap ? *buf->last_with_datap : buf->last;
    if (!chain)
    {
        chain = buf_chain_new(datlen);
        if (!chain){
          pthread_mutex_unlock(&buf->lock);
            return -1;
        }
        buf_chain_insert(buf, chain);
    }
    // 剩余空间
    remain = chain->buffer_len - chain->misalign - chain->off;
    if (remain >= datlen)
    {
        // 直接写入
        memcpy(chain->buffer + chain->misalign + chain->off, data, datlen);
        chain->off += datlen;
        buf->total_len += datlen;
         pthread_mutex_unlock(&buf->lock);
        return 0;
    }
    // 可重排则前移
    if (buf_chain_should_realign(chain, datlen))
    {
        buf_chain_align(chain);
        memcpy(chain->buffer + chain->off, data, datlen);
        chain->off += datlen;
        buf->total_len += datlen;
         pthread_mutex_unlock(&buf->lock);
        return 0;
    }
    // 否则新建扩容节点
    to_alloc = chain->buffer_len;
    if (to_alloc <= BUFFER_CHAIN_MAX_AUTO_SIZE / 2)
        to_alloc <<= 1;
    if (datlen > to_alloc)
        to_alloc = datlen;
    tmp = buf_chain_new(to_alloc);
    if (!tmp){
      pthread_mutex_unlock(&buf->lock);
        return -1;
    }
    if (remain)
    {
        memcpy(chain->buffer + chain->misalign + chain->off, data, remain);
        chain->off += remain;
        buf->total_len += remain;
    }
    data += remain;
    datlen -= remain;
    memcpy(tmp->buffer, data, datlen);
    tmp->off = datlen;
    buf_chain_insert(buf, tmp);
    pthread_mutex_unlock(&buf->lock);
    return 0;
}

/**
 * buf_copyout: 从 buffer 读取数据到 data_out，不删除链节点
 */
static uint32_t
buf_copyout(buffer_t *buf, void *data_out, uint32_t datlen)
{
    buf_chain_t *chain = buf->first;
    char *data = (char *)data_out;
    if (datlen > buf->total_len)
        datlen = buf->total_len;
    if (!datlen)
        return 0;
    uint32_t nread = datlen;
    // 分片拷贝
    while (datlen && chain && datlen >= chain->off)
    {
        memcpy(data, chain->buffer + chain->misalign, chain->off);
        data += chain->off;
        datlen -= chain->off;
        chain = chain->next;
    }
    if (datlen && chain)
    {
        memcpy(data, chain->buffer + chain->misalign, datlen);
    }
    return nread;
}

/**
 * buffer_drain: 从 buffer 前部删除 len 字节数据
 * 1) 删除或调整节点
 * 2) 更新 last_with_datap、total_len、last_read_pos
 */
static int buffer_drain(buffer_t *buf, uint32_t len)
{
    buf_chain_t *chain, *next;
    uint32_t old_len = buf->total_len;
    if (!old_len)
        return 0;
    if (len >= old_len)
    {
        len = old_len;
    }
    buf->total_len -= len;
    uint32_t rem = len;
    for (chain = buf->first; chain && rem >= chain->off; chain = next)
    {
        next = chain->next;
        rem -= chain->off;
        // 清理 last_with_datap 引用
        if (chain == *buf->last_with_datap)
            buf->last_with_datap = &buf->first;
        if (&chain->next == buf->last_with_datap)
            buf->last_with_datap = &buf->first;
        free(chain);
    }
    buf->first = chain;
    if (chain)
    {
        chain->misalign += rem;
        chain->off -= rem;
        buf->last_read_pos -= len;
    }
    return len;
}

/**
 * buffer_remove: 读取并删除数据
 */
int buffer_remove(buffer_t *buf, void *data_out, uint32_t datlen)
{
    pthread_mutex_lock(&buf->lock);
    uint32_t n = buf_copyout(buf, data_out, datlen);
    if (n > 0)
        buffer_drain(buf, n);
     pthread_mutex_unlock(&buf->lock);
    return (int)n;
}

/**
 * buffer_search_kmp: 使用 KMP 算法搜索 sep
 */
static int buffer_search_kmp(buffer_t *buf, const char *sep, int seplen)
{
    int *lps = malloc(sizeof(int) * seplen);
    if (!lps)
        return -1;
    // 构造部分匹配表
    lps[0] = 0;
    int j = 0;
    for (int i = 1; i < seplen; ++i)
    {
        while (j > 0 && sep[i] != sep[j])
            j = lps[j - 1];
        if (sep[i] == sep[j])
            ++j;
        lps[i] = j;
    }
    buf_chain_t *chain = buf->first;
    int idx = 0;
    while (chain)
    {
        for (uint32_t k = chain->misalign; k < chain->misalign + chain->off; ++k)
        {
            char c = (char)chain->buffer[k];
            while (j > 0 && c != sep[j])
                j = lps[j - 1];
            if (c == sep[j])
                ++j;
            ++idx;
            if (j == seplen)
            {
                free(lps);
                return idx;
            }
        }
        chain = chain->next;
    }
    buf->last_read_pos = idx - j;
    free(lps);
    return 0;
}

/**
 * buffer_search: 按长度选择 KMP 或暴力搜索
 */

static bool check_sep(buf_chain_t *chain, int from, const char *sep, int seplen)
{
    for (;;)
    {
        int sz = chain->off - from;
        if (sz >= seplen)
        {
            return memcmp(chain->buffer + chain->misalign + from, sep, seplen) == 0;
        }
        if (sz > 0)
        {
            if (memcmp(chain->buffer + chain->misalign + from, sep, sz))
            {
                return false;
            }
        }
        chain = chain->next;
        sep += sz;
        seplen -= sz;
        from = 0;
    }
}

int buffer_search(buffer_t *buf, const char *sep, const int seplen)
{
    pthread_mutex_lock(&buf->lock);
    if (seplen <= 0 || buf == NULL || buf->total_len < (uint32_t)seplen){
        pthread_mutex_unlock(&buf->lock);
        return -1;
    }
    if (seplen >= REQURIEDLEN_WITH_KMP)
    {

        int ans = buffer_search_kmp(buf, sep, seplen);
        pthread_mutex_unlock(&buf->lock);
        return ans;
    }
    buf_chain_t *chain;
    int i;
    chain = buf->first;
    if (chain == NULL){
       pthread_mutex_unlock(&buf->lock);   
        return 0;
    }
    int bytes = chain->off;
    while (bytes <= buf->last_read_pos)
    {
        chain = chain->next;
        if (chain == NULL){
          pthread_mutex_unlock(&buf->lock);   
            return 0;
        }
        bytes += chain->off;
    }
    bytes -= buf->last_read_pos;
    int from = chain->off - bytes;
    for (i = buf->last_read_pos; i <= buf->total_len - seplen; i++)
    {
        if (check_sep(chain, from, sep, seplen))
        {
            buf->last_read_pos = 0;
            pthread_mutex_unlock(&buf->lock);   
            return i + seplen;
        }
        ++from;
        --bytes;
        if (bytes == 0)
        {
            chain = chain->next;
            from = 0;
            if (chain == NULL)
                break;
            bytes = chain->off;
        }
    }
    buf->last_read_pos = i;
    pthread_mutex_unlock(&buf->lock);
    return 0;
}

/**
 * buffer_read_single_chain: 从 fd 读取数据到 buffer
 * 1) 定位当前尾链（上次写入的链或 buf->last）
 * 2) 计算尾链剩余可写空间
 * 3) 如果没有链块或空间用尽，就分配一个新链
 * 4) 从 fd 读入最多 free_space 字节
 */
int buffer_read_single_chain(buffer_t *buf, int fd)
{
    pthread_mutex_lock(&buf->lock);

    /* 1. 定位当前尾链（上次写入的链或 buf->last） */
    buf_chain_t *chain = *buf->last_with_datap
                        ? *buf->last_with_datap
                        : buf->last;

    /* 2. 计算尾链剩余可写空间 */
    uint32_t free_space = 0;
    if (chain) {
        free_space = chain->buffer_len
                     - chain->misalign
                     - chain->off;
    }

    /* 3. 如果没有链块或空间用尽，就分配一个新链 */
    if (!chain || free_space == 0) {
        /* 新链大小：沿用指数扩容策略，但至少要有 MIN_BUFFER_SIZE 大小 */
        uint32_t alloc = chain
            ? chain->buffer_len * 2
            : MIN_BUFFER_SIZE;
        if (alloc < MIN_BUFFER_SIZE) alloc = MIN_BUFFER_SIZE;
        if (alloc > BUFFER_CHAIN_MAX_AUTO_SIZE)
            alloc = BUFFER_CHAIN_MAX_AUTO_SIZE;

        chain = buf_chain_new(alloc);
        if (!chain) {
            pthread_mutex_unlock(&buf->lock);
            return -1;
        }
        buf_chain_insert(buf, chain);

        /* 更新剩余空间 */
        free_space = chain->buffer_len - chain->misalign;
    }

    /* 4. 从 fd 读入最多 free_space 字节 */
    int n = read(fd,
                     chain->buffer + chain->misalign + chain->off,
                     free_space);
    if (n > 0) {
        chain->off     += (uint32_t)n;
        buf->total_len += (uint32_t)n;
        buf->last_with_datap = &chain;
    }

    pthread_mutex_unlock(&buf->lock);
    return n;
}
