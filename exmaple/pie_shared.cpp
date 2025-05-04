// compile with: -Lbuild -lfoo

#include "../include/sst.hpp"
#include <cstdio>
#include <cstdio>
#include <iostream>
#include <csignal>
#include <dlfcn.h>
#include <unistd.h>

void handle_sigsegv(int sig) {
    std::cerr << "Caught signal: " << sig << "\n";
    auto st = stacktrace::Stacktrace::capture();
    st.print();
    
    _exit(0); // 避免回到不安全的上下文
}

void setup_signal_handler() {
    struct sigaction sa {};
    sa.sa_handler = handle_sigsegv;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGSEGV, &sa, nullptr);
}

extern "C" int add(int, int);

void test_shared_link() {
    add(1, 2);
}

int main(const int argc, const char** argv) {
    setup_signal_handler();
    test_shared_link();
    return 0;
}
