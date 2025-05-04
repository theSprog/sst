// compile with: none
#include "../include/stacktrace.hpp"

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
