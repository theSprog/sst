#include "../src/sst.h"

int main() {
    sst_backtrace bt;
    sst_capture(&bt);
    sst_print_stdout(&bt);
    return 0;
}
