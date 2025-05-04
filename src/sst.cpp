#include "sst.h"
#include "../include/sst.hpp"

#include <cstdio>
#include <cstring>

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
