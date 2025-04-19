#ifndef _chain_buffer_h
#define _chain_buffer_h
#include <stdint.h>

typedef struct buf_chain_s buf_chain_t;
typedef struct buffer_s buffer_t;
// struct buf_chain_s {
//     struct buf_chain_s *next;
//     uint32_t buffer_len;
//     uint32_t misalign;
//     uint32_t off;
//     uint8_t *buffer;
// };

// struct buffer_s {
//     buf_chain_t *first;
//     buf_chain_t *last;
//     buf_chain_t **last_with_datap;
//     uint32_t total_len;
//     uint32_t last_read_pos; // for sep read
// };

//创建buffer
buffer_t * buffer_new(uint32_t sz);
//获取buffer长度
uint32_t buffer_len(buffer_t *buf);
//添加缓冲区
int buffer_add(buffer_t *buf, const void *data, uint32_t datlen);
//获取缓冲区 
int buffer_remove(buffer_t *buf, void *data, uint32_t datlen);
//释放buffer
void buffer_free(buffer_t *buf);
//查询当前是否有分隔符，有则返回第一个分隔符包含前面缓冲区内容的长度
int buffer_search(buffer_t *buf, const char* sep, const int seplen);

uint8_t * buffer_write_atmost(buffer_t *p);

#endif 