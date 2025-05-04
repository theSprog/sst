#ifndef SST_H
#define SST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/// API 版本号，便于未来兼容扩展
#define SST_API_VERSION 1

/// 最大栈帧数（超过将被截断）
#define SST_MAX_FRAMES 32

/// 最大函数名长度（包含 '\0'），超长将以 "..." 截断
#define SST_SYMBOL_NAME_LEN 128

/// 最大模块路径长度（包含 '\0'），超长将以 "..." 截断
#define SST_MODULE_NAME_LEN 256

/// 表示一个解析后的栈帧
typedef struct {
    size_t index;                         ///< 帧编号
    uintptr_t abs_addr;                   ///< 绝对地址
    uintptr_t offset;                     ///< 符号内偏移
    char function[SST_SYMBOL_NAME_LEN];   ///< 函数名，可能被截断为 "...”
    char module[SST_MODULE_NAME_LEN];     ///< 模块路径，可能被截断
    int has_symbol;                       ///< 是否成功解析出符号
} sst_frame;

/// 栈回溯结构体，包含若干帧
typedef struct {
    sst_frame frames[SST_MAX_FRAMES];     ///< 栈帧数组
    size_t size;                          ///< 有效帧数
} sst_backtrace;

/**
 * @brief 捕获当前线程的栈回溯
 * @param out [out] 指向结果结构体，不能为空
 */
void sst_capture(sst_backtrace* out);

/**
 * @brief 解析一个地址对应的符号信息
 * @param addr 要解析的地址（通常来自 backtrace 或函数指针）
 * @param out [out] 结果帧结构体
 */
void sst_resolve(void* addr, sst_frame* out);

/**
 * @brief 打印栈信息到指定文件流
 * @param trace 栈结构体（来自 sst_capture）
 * @param file 目标文件流，例如 stderr / stdout / fopen 的句柄
 */
void sst_print(const sst_backtrace* trace, FILE* file);

/**
 * @brief 打印栈信息到标准错误流
 * @param trace 栈结构体
 */
void sst_print_stderr(const sst_backtrace* trace);

/**
 * @brief 打印栈信息到标准输出流
 * @param trace 栈结构体
 */
void sst_print_stdout(const sst_backtrace* trace);

#ifdef __cplusplus
}
#endif

#endif // SST_H
