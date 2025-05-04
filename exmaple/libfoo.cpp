#include <vector>

void add2(std::vector<int> vec) {
    int* p = nullptr;
    int a = *p; // 崩溃
    (void)a;
}

int add1() {
    add2({123});
    return 0;
}

extern "C" int add(int a, int b) {
    return add1();
}
