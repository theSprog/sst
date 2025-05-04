// compile with: -no-pie -static

#include "../include/stacktrace.hpp"
#include <cstdio>

void C() {
    auto bt = stacktrace::Stacktrace::capture();
    bt.print();
    
}

void B() {
    C();
}

void A() {
    B();
}

int main(const int argc, const char** argv) {
    A();
    return 0;
}
