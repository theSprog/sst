// compile with: nothing

#include "../include/sst.hpp"
#include <sys/types.h>

int main(const int argc, const char** argv) {
    pid_t target_pid;
    std::cout << "Enter you target pid: " << std::endl;
    std::cin >> target_pid;
    std::vector<void *> addr_batch;
    stacktrace::Stacktrace::resolve_to_raw_on_pid(addr_batch, target_pid);
    // there should be print nothing becase it just load modules
    return 0;
}
