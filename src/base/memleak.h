/*
调用方式，确保可执行程序目录下有一个mem文件夹，所有泄漏信息会输出到该文件夹下
格式类似如下
[+]0xb3502, addr: 0x5625563262d0, size: 4


addr2line -e (exe_path) -f [+](地址)
例如
addr2line -e ./socketpool_test -f 0xb3502


注意 一些不带调试信息的外部库，是没办法追踪的 显示会是?? 不需要关心
*/


#pragma once
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>


#ifdef __cplusplus
extern "C" {
#endif

// 全局 hook 状态和真实函数指针
// 使用线程局部变量防止递归调用冲突
__thread int hook_in_malloc = 0;
__thread int hook_in_free   = 0;

// 类型定义真实的 malloc/free 函数指针类型
typedef void *(*malloc_t)(size_t size);
typedef void (*free_t)(void *ptr);

// 静态全局变量保存真实的函数指针
static malloc_t real_malloc = NULL;
static free_t   real_free   = NULL;

// 用于初始化 hook 的互斥锁和标志
static int hooks_initialized = 0;
static pthread_mutex_t hook_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * init_hook - 初始化 hook 所需的真实函数指针
 * 确保 dlsym 调用只执行一次，且在多线程环境下安全
 */
void init_hook(void) {
    pthread_mutex_lock(&hook_mutex);
    if (!hooks_initialized) {
        real_malloc = (malloc_t)dlsym(RTLD_NEXT, "malloc");
        real_free   = (free_t)dlsym(RTLD_NEXT, "free");
        hooks_initialized = 1;
    }
    pthread_mutex_unlock(&hook_mutex);
}

/*
 * ConvertToELF - 转换传入地址为 ELF 文件内的相对地址
 * 对 dladdr1 调用增加错误检查，若失败则返回原地址
 */
void *ConvertToELF(void *addr) {
    Dl_info info;
    struct link_map *link = NULL;
    if (dladdr1(addr, &info, (void **)&link, RTLD_DL_LINKMAP) == 0 || link == NULL) {
        return addr; // 若获取失败，返回原地址
    }
    return (void *)((size_t)addr - link->l_addr);
}

/*
 * Hook 版本的 malloc 函数
 * 保存分配记录到 "./mem/<address>.mem" 文件中，文件名以分配地址命名
 */
void *malloc(size_t size) {
    // 如未初始化，先初始化 hook
    if (!hooks_initialized) {
        init_hook();
    }
    
    // 如果已进入 hook，则直接调用真实 malloc 防止递归
    if (hook_in_malloc) {
        return real_malloc(size);
    }
    
    hook_in_malloc = 1;
    void *p = real_malloc(size);
    
    // 获取调用者返回地址，用于转换符号
    void *caller = __builtin_return_address(0);
    
    // 构造文件名，要求 "./mem" 目录事先存在
    char filename[128] = {0};
    snprintf(filename, sizeof(filename), "./mem/%p.mem", p);
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        // 打开文件失败时恢复 hook 标志，并直接释放内存
        hook_in_malloc = 0;
        real_free(p);
        return NULL;
    }
    
    // 写入调用者地址（转换为 ELF 相对地址）、分配地址和大小信息
    fprintf(fp, "[+]%p, addr: %p, size: %zu\n", ConvertToELF(caller), p, size);
    fflush(fp);
    fclose(fp);  // 关闭文件，防止泄漏
    
    hook_in_malloc = 0;
    return p;
}

/*
 * Hook 版本的 free 函数
 * 根据传入内存地址构造文件名，删除对应记录文件，防止重复释放
 */
void free(void *ptr) {
    // 如未初始化，先初始化 hook
    if (!hooks_initialized) {
        init_hook();
    }
    
    // 如果已进入 hook，则直接调用真实 free 防止递归
    if (hook_in_free) {
        real_free(ptr);
        return;
    }
    
    hook_in_free = 1;
    
    char filename[128] = {0};
    snprintf(filename, sizeof(filename), "./mem/%p.mem", ptr);
    // 使用 unlink 删除记录文件
    unlink(filename) ;
    real_free(ptr);
    hook_in_free = 0;
}


__attribute__((constructor)) static void auto_init_hook() {
    init_hook();
}
#ifdef __cplusplus
}
#endif 
