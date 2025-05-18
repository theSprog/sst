#include "sst.h"
#include "../include/sst.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

// this .cpp would compile to .so/.a, so `using namespace` is ok
using namespace stacktrace;

/// 帮助函数：安全填充 sst_frame
static void fill_frame_info(const ResolvedFrame& src, sst_frame* dst) {
    dst->index = src.index;
    dst->abs_addr = src.abs_addr;
    dst->offset = src.offset;
    dst->has_symbol = src.has_symbol;

    // 拷贝函数名，尾部可能加 "..."
    if (src.function.size() >= SST_SYMBOL_NAME_LEN - 4) {
        snprintf(dst->function,
                 SST_SYMBOL_NAME_LEN,
                 "%.*s...",
                 static_cast<int>(SST_SYMBOL_NAME_LEN - 4 - 1),
                 src.function.c_str());
    } else {
        snprintf(dst->function, SST_SYMBOL_NAME_LEN, "%s", src.function.c_str());
    }

    // 拷贝模块名，尾部可能加 "..."
    if (src.module.size() >= SST_MODULE_NAME_LEN - 4) {
        snprintf(dst->module,
                 SST_MODULE_NAME_LEN,
                 "%.*s...",
                 static_cast<int>(SST_MODULE_NAME_LEN - 4 - 1),
                 src.module.c_str());
    } else {
        snprintf(dst->module, SST_MODULE_NAME_LEN, "%s", src.module.c_str());
    }
}

void sst_capture(sst_backtrace* out) {
    if (! out) return;

    Stacktrace st = Stacktrace::capture(SST_MAX_FRAMES);
    const auto& frames = st.get_frames();

    out->size = std::min(frames.size(), static_cast<size_t>(SST_MAX_FRAMES));
    for (size_t i = 0; i < out->size; ++i) {
        fill_frame_info(frames[i], &out->frames[i]);
    }
}

void sst_resolve(void* addr, sst_frame* out) {
    if (! addr || ! out) return;

    auto frame = Stacktrace::resolve(addr);
    fill_frame_info(frame, out);
}

void sst_clear_modules_cache() {
    Stacktrace::clear_modules_cache();
}

void sst_print(const sst_backtrace* trace, FILE* file) {
    if (! trace || ! file) return;

    size_t n = (trace->size > SST_MAX_FRAMES) ? SST_MAX_FRAMES : trace->size;
    for (size_t i = 0; i < n; ++i) {
        const sst_frame* frame = &trace->frames[i];
        if (frame->has_symbol) {
            fprintf(file,
                    "[%zu] %s+0x%lx in %s (%p)\n",
                    frame->index,
                    frame->function,
                    frame->offset,
                    frame->module,
                    reinterpret_cast<void*>(frame->abs_addr));
        } else {
            fprintf(file,
                    "[%zu] (no symbol) in %s (%p)\n",
                    frame->index,
                    frame->module,
                    reinterpret_cast<void*>(frame->abs_addr));
        }
    }
}

void sst_print_stderr(const sst_backtrace* trace) {
    sst_print(trace, stderr);
}

void sst_print_stdout(const sst_backtrace* trace) {
    sst_print(trace, stdout);
}

void sst_resolve_to_raw(void* addr, sst_raw_frame* out) {
    if (! addr || ! out) return;

    RawFrame rf = Stacktrace::resolve_to_raw(addr);

    out->abs_addr = reinterpret_cast<uintptr_t>(rf.abs_addr);
    out->offset = rf.offset;
    out->has_symbol = rf.has_symbol;

    if (! rf.module.empty()) {
        // 使用 strdup 复制模块名
        out->module = strdup(rf.module.c_str());
    } else {
        out->module = nullptr;
    }
}

void sst_resolve_raw_batch(void** addrs, size_t count, sst_raw_frame* outs) {
    if (! addrs || ! outs || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        sst_resolve_to_raw(addrs[i], &outs[i]);
    }
}

void sst_resolve_batch_on_pid(pid_t target_pid, void** addrs, size_t count, sst_frame* outs) {
    if (! addrs || ! outs || count == 0) return;

    std::vector<void*> addr_batch(addrs, addrs + count);

    // 执行符号解析（注意：必须确保返回结果数量匹配）
    std::vector<ResolvedFrame> frames = Stacktrace::resolve_on_pid(addr_batch, target_pid);

    for (size_t i = 0; i < count; ++i) {
        fill_frame_info(frames[i], &outs[i]);
    }
}

void sst_resolve_raw_batch_on_pid(pid_t target_pid, void** addrs, size_t count, sst_raw_frame* outs) {
    if (! addrs || ! outs || count == 0) return;

    std::vector<void*> addr_batch(addrs, addrs + count);

    // 执行符号解析（注意：必须确保返回结果数量匹配）
    std::vector<RawFrame> rawframes = Stacktrace::resolve_to_raw_on_pid(addr_batch, target_pid);

    for (size_t i = 0; i < count; ++i) {
        const RawFrame& rf = rawframes[i];
        sst_raw_frame& out = outs[i];

        out.abs_addr = reinterpret_cast<uintptr_t>(rf.abs_addr);
        out.offset = rf.offset;
        out.has_symbol = rf.has_symbol;

        // 避免重复 strdup 空字符串
        if (! rf.module.empty()) {
            out.module = strdup(rf.module.c_str()); // 必须由调用方负责释放
        } else {
            out.module = nullptr;
        }
    }
}

void sst_free_raw_frames(sst_raw_frame* frames, size_t count) {
    if (! frames || count == 0) return;

    for (size_t i = 0; i < count; ++i) {
        if (frames[i].module) {
            free(frames[i].module);
            frames[i].module = nullptr;
        }
    }
}
