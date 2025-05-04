// compile with: -no-pie

#include "../include/stacktrace.hpp"
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

typedef int (*AddFunc)(int, int);

void test_dlopen() {
    void* handle = dlopen("./libfoo.so", RTLD_NOW);
    if (!handle) {
        std::cerr << "dlopen failed: " << dlerror() << "\n";
        return;
    }

    AddFunc add = reinterpret_cast<AddFunc>(dlsym(handle, "add"));
    if (!add) {
        std::cerr << "dlsym failed\n";
        return;
    }

    add(3, 4);
}

int main(const int argc, const char** argv) {
    setup_signal_handler();
    test_dlopen();
    return 0;
}
