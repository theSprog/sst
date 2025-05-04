English version is [here](./README_en.md)

`sst((simple stack trace))` 是一个轻量级、零依赖、**header-only** 的 C++ 栈回溯库，专为低侵入、高兼容性而设计。它支持：

- ✅ **不依赖 DWARF**：仅使用符号表（`.symtab` / `.dynsym`），不依赖 dwarf 调试信息
- ✅ **pie / no-pie**：自动识别主程序是否 pie 类型，准确处理地址重定位
- ✅ **静态 / 动态链接**：在 `-static`、`-no-pie`、`-pie`、`-lxxx` 等构建下均可正常工作
- ✅ **动态库解析**：支持 `dlopen()` 动态加载模块的符号解析
- ✅ **C API 导出**：可通过 `libsst.so` / `libsst.a` 提供给纯 C 项目调用

---



## 📁 项目结构

```

.
├── include/
│   └── sst.hpp          # ✅ 核心头文件，header-only，可直接引入使用
├── src/
│   ├── sst.cpp          # 🔁 C API 实现
│   └── sst.h            # 🔁 C API 头文件（便于其他语言如 Python FFI）
├── exmaple/
│   └── \*.cpp            # 📦 多种构建配置下的例子（pie / no-pie / static / shared / dlopen 等）
├── test/
│   ├── test\_capi.c      # 🧪 使用 C API 的测试程序
└── README.md            # 📖 当前文档

````

---



## 🔧 快速开始（C++）

你只需一个头文件：

```cpp
#include "sst.hpp"

int main() {
    stacktrace::Stacktrace st = stacktrace::Stacktrace::capture();
    st.print();  // 输出当前栈帧
}
````

---



## 🌐 C API 用法

你可以构建 `libsst.a` 或 `libsst.so` 来为 C 项目或其他语言提供支持：

```c
#include "sst.h"

int main() {
    sst_backtrace trace;
    sst_capture(&trace);
    sst_print(&trace);
    return 0;
}
```

构建参考：

```bash
cd src && make       # 构建静态和动态库

# 如果想要测试
cd test && make      # 编译 C 测试代码
```

---



## 💡 示例运行

你可以参考 `exmaple/` 目录中的样例测试不同构建配置下的行为：

* `pie.cpp`, `nopie.cpp`, `static.cpp`：测试主程序构建方式
* `pie_dlopen.cpp`, `nopie_dlopen.cpp`：测试 `dlopen()` 场景
* `*_shared*.cpp`：测试链接动态库后能否成功回溯库中函数

---



## 🛠️ 技术原理

* 使用 `dl_iterate_phdr` 遍历所有加载模块（包括主程序和动态库）
* 基于 ELF 文件格式解析 `.symtab` 和 `.dynsym`
* PIE 程序使用 `dlpi_addr + st_value`，非 PIE 直接使用 `st_value`
* 对于 no-pie 和 static 构建，基地址通过 `/proc/self/maps` 解析出


## 📬 联系作者
欢迎提 Issue 或 PR 进行反馈与贡献（如果代码有 bug，请务必告诉我😘）！
