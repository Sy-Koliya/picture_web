#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "buffer.h"

// 链节点结构体：用于存储一段数据
struct buf_chain_s {
    struct buf_chain_s *next;    // 指向下一个链节点
    uint32_t buffer_len;         // 本节点分配的缓冲区总长度
    uint32_t misalign;           // 数据起始位置偏移（已读/删除的数据区间）
    uint32_t off;                // 本节点中有效数据长度
    uint8_t *buffer;             // 指向实际存放数据的内存
};

// 缓冲区结构体：由多个链节点组成的环状链表
struct buffer_s {
    buf_chain_t *first;          // 链表头
    buf_chain_t *last;           // 链表尾，便于快速追加
    buf_chain_t **last_with_datap;// 指向最近一次写入数据的节点指针引用，优化插入和清理
    uint32_t total_len;          // 缓冲区中所有链节点的有效数据总长度
    uint32_t last_read_pos;      // 上次搜索时匹配停留的全局偏移
};

// 计算链节点可写剩余空间
#define CHAIN_SPACE_LEN(ch) ((ch)->buffer_len - ((ch)->misalign + (ch)->off))
#define MIN_BUFFER_SIZE 1024                     // 链节点最小分配尺寸
#define MAX_TO_COPY_IN_EXPAND 4096               // 扩容时一次最多复制的数据量
#define BUFFER_CHAIN_MAX_AUTO_SIZE 4096          // 自动扩容阈值
#define MAX_TO_REALIGN_IN_EXPAND 2048            // 最大重排阈值
#define BUFFER_CHAIN_MAX (16*1024*1024)          // 单链节点最大容量 16MB
#define BUFFER_CHAIN_EXTRA(t, c) (t *)((buf_chain_t *)(c) + 1) // 计算链节点数据区起始地址
#define BUFFER_CHAIN_SIZE sizeof(buf_chain_t)    // 链节点头结构大小
#define REQURIEDLEN_WITH_KMP 8                   // 小于此长度使用暴力搜索，以上使用 KMP

/**
 * ZERO_CHAIN: 重置 buffer 结构
 * 清空链表相关指针和统计信息
 */
static inline void
ZERO_CHAIN(buffer_t *dst) {
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
buffer_len(buffer_t *buf) {
    return buf->total_len;
}

/**
 * buffer_new: 创建并初始化 buffer 对象
 * sz 参数暂不使用，可用于预留容量扩展
 */
buffer_t *
buffer_new(uint32_t sz) {
    (void)sz;
    buffer_t *buf = (buffer_t *)malloc(sizeof(buffer_t));
    if (!buf) {
        return NULL;
    }
    memset(buf, 0, sizeof(*buf));
    buf->last_with_datap = &buf->first;
    return buf;
}

/**
 * buf_chain_new: 分配并初始化一个链节点
 * size: 期望存储的数据长度
 * 返回新节点指针或 NULL
 */
static buf_chain_t *
buf_chain_new(uint32_t size) {
    buf_chain_t *chain;
    uint32_t to_alloc;
    if (size > BUFFER_CHAIN_MAX - BUFFER_CHAIN_SIZE)
        return NULL;
    size += BUFFER_CHAIN_SIZE;
    // 取 2 的幂次增长，至少 MIN_BUFFER_SIZE
    if (size < BUFFER_CHAIN_MAX/2) {
        to_alloc = MIN_BUFFER_SIZE;
        while (to_alloc < size) {
            to_alloc <<= 1;
        }
    } else {
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
buf_chain_free_all(buf_chain_t *chain) {
    buf_chain_t *next;
    while (chain) {
        next = chain->next;
        free(chain);
        chain = next;
    }
}

/**
 * buffer_free: 释放 buffer 及其中所有链节点
 */
void
buffer_free(buffer_t *buf) {
    if (buf) {
        buf_chain_free_all(buf->first);
        ZERO_CHAIN(buf);
        free(buf);
    }
}

/**
 * free_empty_chains: 清理 last_with_datap 之后的空链节点
 * 返回清理后可插入的位置引用
 */
static buf_chain_t **
free_empty_chains(buffer_t *buf) {
    buf_chain_t **ch = buf->last_with_datap;
    while ((*ch) && (*ch)->off != 0)
        ch = &(*ch)->next;
    if (*ch) {
        buf_chain_free_all(*ch);
        *ch = NULL;
    }
    return ch;
}

/**
 * buf_chain_insert: 将节点插入 buffer 链中，并更新统计
 */
static void
buf_chain_insert(buffer_t *buf, buf_chain_t *chain) {
    if (*buf->last_with_datap == NULL) {
        // 首次插入
        buf->first = buf->last = chain;
    } else {
        // 清理空链后插入
        buf_chain_t **chp = free_empty_chains(buf);
        *chp = chain;
        if (chain->off)
            buf->last_with_datap = chp;
        buf->last = chain;
    }
    buf->total_len += chain->off;
}

static int
buf_chain_should_realign(buf_chain_t *chain, uint32_t datlen)
{
    return chain->buffer_len - chain->off >= datlen &&
        (chain->off < chain->buffer_len / 2) &&
        (chain->off <= MAX_TO_REALIGN_IN_EXPAND);
}

static void
buf_chain_align(buf_chain_t *chain) {
    memmove(chain->buffer, chain->buffer + chain->misalign, chain->off);
    chain->misalign = 0;
}

/**
 * buffer_add: 向 buffer 追加数据
 * 1) 尝试写入 last_with_datap 所指节点
 * 2) 若空间不足，判断是否可重排或扩容
 * 3) 最终新建节点并插入
 */
int buffer_add(buffer_t *buf, const void *data_in, uint32_t datlen) {
    buf_chain_t *chain, *tmp;
    const char *data = (char *)data_in;
    uint32_t remain, to_alloc;
    if (datlen > BUFFER_CHAIN_MAX - buf->total_len) {
        return -1;  // 超出最大容量
    }
    // 定位写入节点
    chain = *buf->last_with_datap ? *buf->last_with_datap : buf->last;
    if (!chain) {
        chain = buf_chain_new(datlen);
        if (!chain) return -1;
        buf_chain_insert(buf, chain);
    }
    // 剩余空间
    remain = chain->buffer_len - chain->misalign - chain->off;
    if (remain >= datlen) {
        // 直接写入
        memcpy(chain->buffer + chain->misalign + chain->off, data, datlen);
        chain->off += datlen;
        buf->total_len += datlen;
        return 0;
    }
    // 可重排则前移
    if (buf_chain_should_realign(chain, datlen)) {
        buf_chain_align(chain);
        memcpy(chain->buffer + chain->off, data, datlen);
        chain->off += datlen;
        buf->total_len += datlen;
        return 0;
    }
    // 否则新建扩容节点
    to_alloc = chain->buffer_len;
    if (to_alloc <= BUFFER_CHAIN_MAX_AUTO_SIZE/2)
        to_alloc <<= 1;
    if (datlen > to_alloc)
        to_alloc = datlen;
    tmp = buf_chain_new(to_alloc);
    if (!tmp) return -1;
    if (remain) {
        memcpy(chain->buffer + chain->misalign + chain->off, data, remain);
        chain->off += remain;
        buf->total_len += remain;
    }
    data += remain;
    datlen -= remain;
    memcpy(tmp->buffer, data, datlen);
    tmp->off = datlen;
    buf_chain_insert(buf, tmp);
    return 0;
}

/**
 * buf_copyout: 从 buffer 读取数据到 data_out，不删除链节点
 */
static uint32_t
buf_copyout(buffer_t *buf, void *data_out, uint32_t datlen) {
    buf_chain_t *chain = buf->first;
    char *data = (char *)data_out;
    if (datlen > buf->total_len)
        datlen = buf->total_len;
    if (!datlen) return 0;
    uint32_t nread = datlen;
    // 分片拷贝
    while (datlen && chain && datlen >= chain->off) {
        memcpy(data, chain->buffer + chain->misalign, chain->off);
        data += chain->off;
        datlen -= chain->off;
        chain = chain->next;
    }
    if (datlen && chain) {
        memcpy(data, chain->buffer + chain->misalign, datlen);
    }
    return nread;
}

/**
 * buffer_drain: 从 buffer 前部删除 len 字节数据
 * 1) 删除或调整节点
 * 2) 更新 last_with_datap、total_len、last_read_pos
 */
static int buffer_drain(buffer_t *buf, uint32_t len) {
    buf_chain_t *chain, *next;
    uint32_t old_len = buf->total_len;
    if (!old_len) return 0;
    if (len >= old_len) {
        buf_chain_free_all(buf->first);
        ZERO_CHAIN(buf);
        return old_len;
    }
    buf->total_len -= len;
    uint32_t rem = len;
    for (chain = buf->first; chain && rem >= chain->off; chain = next) {
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
    if (chain) {
        chain->misalign += rem;
        chain->off -= rem;
        buf->last_read_pos -= len;
    }
    return len;
}

/**
 * buffer_remove: 读取并删除数据
 */
int buffer_remove(buffer_t *buf, void *data_out, uint32_t datlen) {
    uint32_t n = buf_copyout(buf, data_out, datlen);
    if (n > 0) buffer_drain(buf, n);
    return (int)n;
}

/**
 * buffer_search_kmp: 使用 KMP 算法搜索 sep
 */
static int buffer_search_kmp(buffer_t *buf, const char *sep, int seplen) {
    int *lps = malloc(sizeof(int) * seplen);
    if (!lps) return -1;
    // 构造部分匹配表
    lps[0] = 0; int j = 0;
    for (int i = 1; i < seplen; ++i) {
        while (j > 0 && sep[i] != sep[j]) j = lps[j-1];
        if (sep[i] == sep[j]) ++j;
        lps[i] = j;
    }
    buf_chain_t *chain = buf->first;
    int idx = 0;
    while (chain) {
        for (uint32_t k = chain->misalign; k < chain->misalign + chain->off; ++k) {
            char c = (char)chain->buffer[k];
            while (j > 0 && c != sep[j]) j = lps[j-1];
            if (c == sep[j]) ++j;
            ++idx;
            if (j == seplen) {
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

static bool check_sep(buf_chain_t * chain, int from, const char *sep, int seplen) {
    for (;;) {
        int sz = chain->off - from;
        if (sz >= seplen) {
            return memcmp(chain->buffer + chain->misalign + from, sep, seplen) == 0;
        }
        if (sz > 0) {
            if (memcmp(chain->buffer + chain->misalign + from, sep, sz)) {
                return false;
            }
        }
        chain = chain->next;
        sep += sz;
        seplen -= sz;
        from = 0;
    }
}

int buffer_search(buffer_t *buf, const char* sep, const int seplen) {
    if (seplen <= 0 || buf == NULL || buf->total_len < (uint32_t)seplen)
    return -1;
    if(seplen >= REQURIEDLEN_WITH_KMP){
        return buffer_search_kmp(buf ,sep ,seplen);
    }
    buf_chain_t *chain;
    int i;
    chain = buf->first;
    if (chain == NULL)
        return 0;
    int bytes = chain->off;
    while (bytes <= buf->last_read_pos) {
        chain = chain->next;
        if (chain == NULL)
            return 0;
        bytes += chain->off;
    }
    bytes -= buf->last_read_pos;
    int from = chain->off - bytes;
    for (i = buf->last_read_pos; i <= buf->total_len - seplen; i++) {
        if (check_sep(chain, from, sep, seplen)) {  
            buf->last_read_pos = 0;
            return i+seplen;
        }
        ++from;
        --bytes;
        if (bytes == 0) {
            chain = chain->next;
            from = 0;
            if (chain == NULL)
                break;
            bytes = chain->off;
        }
    }
    buf->last_read_pos = i;
    return 0;
}


/**
 * buffer_write_atmost - 获取 buffer 首节点的可写内存指针
 *
 *  在 buffer 中查找或创建一块大于等于 total_len 大小的连续内存区域
 * 
 * 以便用户可以直接写入数据，然后再调用 buffer_add 更新相关长度。
 * 
 *   核心思路：
 * 1. 若首节点的 off >= total_len，说明首节点已有足够空间存放所有数据，
 *    此时直接返回首节点 buffer + misalign。  
 * 2. 否则计算 remaining = total_len - 首节点已用 off，
 *    并尝试遍历后续节点累加其 off，直到 remaining <= 某一节点 off，
 *    用于后续是否能够重用首节点前缀空间。  
 * 3. 如果首节点 (buffer_len - misalign) >= total_len，则说明首节点
 *    有足够预留空间，无需新建节点：  
 *      - 记录原 off，设置 tmp 指向首节点，  
 *      - 将 tmp->off 扩展为 total_len，  
 *      - size 减去 old_off 后仍余需复制的数据，  
 *      - chain = 首节点->next，后续合并逻辑继续进行。  
 * 4. 否则，新建一个大小为 total_len 的链节点 tmp，
 *    将其接入 p->first，后续将数据拷贝到 tmp。  
 * 5. last_with_data 保存调用前最后写入数据的节点，
 *    在拷贝并释放旧节点时，若释放了该节点，需要重置 last_with_datap。  
 * 6. 拷贝过程：遍历 chain 节点，直到 size < chain->off，
 *    每次 memcpy(chain->off) 并 free(chain)，更新 buffer 指针和 size。  
 * 7. 若最后 chain != NULL，则拷贝剩余 size 字节到 chain，
 *    并更新 chain->misalign 与 chain->off；否则，更新 p->last = tmp。  
 * 8. 将 tmp->next 指向 chain，完成链表重组；  
 * 9. 根据 removed_last_with_data / removed_last_with_datap 标志
 *    来更新 p->last_with_datap 的引用，确保其指向最近写入的节点。  
 *
 * 返回值：
 *   指向新节点或首节点中，misalign 位置后的可写起始地址
 */
uint8_t * buffer_write_atmost(buffer_t *p) {
    buf_chain_t *chain, *next, *tmp, *last_with_data;
    uint8_t *buffer;
    uint32_t remaining;
    int removed_last_with_data = 0;
    int removed_last_with_datap = 0;

    // 1. 获取首节点与总长度
    chain = p->first;
    uint32_t size = p->total_len;

    // 2. 若首节点已有足够空间，直接返回该位置
    if (chain->off >= size) {
        return chain->buffer + chain->misalign;
    }

    // 3. 计算除首节点外剩余数据长度 remaining
    remaining = size - chain->off;
    for (tmp = chain->next; tmp; tmp = tmp->next) {
        if (tmp->off >= remaining)
            break;
        remaining -= tmp->off;
    }
    
    // 4. 判断首节点是否可重用
    if (chain->buffer_len - chain->misalign >= size) {
        // 4.1 首节点空间足够：扩展 off 并调整 size、buffer 起始点
        size_t old_off = chain->off;
        buffer = chain->buffer + chain->misalign + chain->off;
        tmp = chain;
        tmp->off = size;
        size -= old_off;
        chain = chain->next;
    } else {
        // 4.2 首节点空间不足：新建 tmp 节点并接入头部
        tmp = buf_chain_new(size);
        if (!tmp)
            return NULL;
        buffer = tmp->buffer;
        tmp->off = size;
        p->first = tmp;
    }

    // 5. 记录原最后写入数据节点
    last_with_data = *p->last_with_datap;
    // 6. 按节点拷贝数据到 tmp，并释放旧节点
    for (; chain && size >= chain->off; chain = next) {
        next = chain->next;
        memcpy(buffer, chain->buffer + chain->misalign, chain->off);
        size -= chain->off;
        buffer += chain->off;
        if (chain == last_with_data)
            removed_last_with_data = 1;
        if (&chain->next == p->last_with_datap)
            removed_last_with_datap = 1;
        free(chain);
    }

    // 7. 处理剩余不足一个节点大小的数据
    if (chain) {
        memcpy(buffer, chain->buffer + chain->misalign, size);
        chain->misalign += size;
        chain->off -= size;
    } else {
        // 所有旧节点均已合并，更新尾指针
        p->last = tmp;
    }

    // 8. 完成新链节点指向
    tmp->next = chain;

    // 9. 更新 last_with_datap 引用，防止指向已释放节点
    if (removed_last_with_data) {
        p->last_with_datap = &p->first;
    } else if (removed_last_with_datap) {
        if (p->first->next && p->first->next->off)
            p->last_with_datap = &p->first->next;
        else
            p->last_with_datap = &p->first;
    }

    // 返回可写区域起始地址
    return tmp->buffer + tmp->misalign;
}