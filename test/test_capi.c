#include "../src/sst.h"

int main() {
    sst_backtrace bt;
    sst_capture(&bt);
    sst_print_stdout(&bt);
    sst_print(&bt, stderr); // Print to a custom file stream

    // Extract raw frame info (can be used with addr2line)
    void* pcs[SST_MAX_FRAMES];
    sst_raw_frame raw[SST_MAX_FRAMES];
    for (size_t i = 0; i < bt.size; ++i) {
        pcs[i] = (void*)bt.frames[i].abs_addr;
    }
    sst_resolve_raw_batch(pcs, bt.size, raw);

    for (size_t i = 0; i < bt.size; ++i) {
        printf("addr: 0x%lx, offset: 0x%lx, module: %s\n",
               (unsigned long)raw[i].abs_addr,
               (unsigned long)raw[i].offset,
               raw[i].module ?: "<unknown>");
    }

    sst_free_raw_frames(raw, bt.size); // Free allocated module strings
    return 0;
}
