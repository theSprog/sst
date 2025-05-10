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

/// 表示一个完全解析后的栈帧
typedef struct sst_frame {
    size_t index;                       ///< 帧编号
    uintptr_t abs_addr;                 ///< 绝对地址
    uintptr_t offset;                   ///< 符号内偏移
    char function[SST_SYMBOL_NAME_LEN]; ///< 函数名，可能被截断为 "...”
    char module[SST_MODULE_NAME_LEN];   ///< 模块路径，可能被截断
    int has_symbol;                     ///< 是否成功解析出符号
} sst_frame;

/// 栈回溯的原始帧结构：仅包含地址信息和所属模块
typedef struct sst_raw_frame {
    uintptr_t abs_addr;       ///< 绝对地址（虚拟地址）
    uintptr_t offset;         ///< 相对于模块的偏移
    int has_symbol;           ///< 是否成功解析符号
    char* module;           ///< 模块名，C 字符串
} sst_raw_frame;

/// 栈回溯结构体，包含若干帧
typedef struct sst_backtrace {
    sst_frame frames[SST_MAX_FRAMES]; ///< 栈帧数组
    size_t size;                      ///< 有效帧数
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

/**
 * @brief 将原始地址解析为 sst_raw_frame 结构体（例如用户态或动态加载模块）
 * @param addr 绝对地址（虚拟地址）
 * @param out [out] 结构体，内部自动填充 module/offset 等
 * @note 若 module 为变长字段，请在 out 指向的内存区域预留足够空间
 */
void sst_resolve_to_raw(void* addr, sst_raw_frame* out);

/**
 * @brief 批量释放一组 sst_raw_frame 中动态分配的模块名
 * 
 * 注意：仅释放每个 frame 的 module 字符串，不释放数组本身。
 * 若数组由 malloc 分配，需由调用者自行释放。
 *
 * @param frames sst_raw_frame 数组指针（不可为 NULL）
 * @param count 数组中的元素个数
 */
void sst_free_raw_frames(sst_raw_frame* frames, size_t count);

/**
 * @brief 将一批地址批量转换为原始帧信息
 * @param addrs 地址数组
 * @param count 地址个数
 * @param out [out] 输出数组，应至少具有 count 个元素空间
 */
void sst_resolve_raw_batch(void** addrs, size_t count, sst_raw_frame* outs);

#ifdef __cplusplus
}
#endif

#endif // SST_H
