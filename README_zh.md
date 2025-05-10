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
│   └── *.cpp            # 📦 多种构建配置下的例子（pie / no-pie / static / shared / dlopen 等）
├── test/
│   ├── test_capi.c      # 🧪 使用 C API 的测试程序
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



### Raw Frame 结构体

> 💡 RawFrame 是一种更低层级的数据结构，这类信息特别适用于配合 `addr2line` 等命令行工具进行二次解析，例如
>
> `addr2line -e /lib/libc.so.6 0x1234`

如果你希望配合 `addr2line` 或构建自定义回溯展示工具，可以进一步调用 `resolve_to_raw()` 或 `get_raw_frames()` 获取绝对地址、偏移量和模块信息

```c++
#include "sst.hpp"

int main() {
    using stacktrace::Stacktrace;
    using stacktrace::RawFrame;

    Stacktrace st = Stacktrace::capture();
    std::vector<RawFrame> raw = st.get_raw_frames();

    for (const auto& f : raw) {
        printf("abs: 0x%lx, offset: 0x%lx, module: %s\n",
               (unsigned long)f.abs_addr,
               (unsigned long)f.offset,
               f.module.c_str());
    }

    // 也可单独解析任意地址
    RawFrame one = Stacktrace::resolve_to_raw((void*)main);
}
```



## 🌐 C API 用法

你可以构建 `libsst.a` 或 `libsst.so` 来为 C 项目或其他语言提供支持：

```c
#include "sst.h"

int main() {
    sst_backtrace bt;
    sst_capture(&bt);
    sst_print_stdout(&bt);
    sst_print(&bt, stderr); // 输出到自定义文件

    // 批量获取 raw 栈帧（可配合 addr2line 使用）
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

    sst_free_raw_frames(raw, bt.size); // 释放 module 字符串
    return 0;
}
```

> 💡为了准确传递 **模块名路径**，`module` 必须是 `char*` 字符串，而不能是定长数组。否则可能会被截断，影响调试效果。所以所有 `sst_raw_frame` 中的 `module` 字符串都由库内部 `strdup()` 分配，使用完后需手动释放，避免内存泄漏。

---



## C库构建参考

```bash
cd src && make       # 构建静态和动态库

# 如果想要测试
cd test && make      # 编译 C 测试代码
```

---



## 🧼 资源管理（C）

所有通过 `sst_resolve_to_raw()` 或 `sst_resolve_raw_batch()` 得到的 `sst_raw_frame.module` 都必须 **手动释放**：

```c
sst_raw_frame frame;
sst_resolve_to_raw(ptr, &frame);
// 使用 frame.module ...
sst_free_raw_frames(&frame, 1);
```

对于批量情况，也有 `sst_free_raw_frames` 可用， 但它只会释放每一个 `sst_raw_frame` 的 `module` 字段不会释放数组本体， 是否释放数组本体由用户决定：

```c
sst_raw_frame* arr = malloc(sizeof(sst_raw_frame) * count);
sst_resolve_raw_batch(addrs, count, arr);
// 使用 arr[i].module ...
sst_free_raw_frames(arr, count);
free(arr); // 可选：释放数组本体
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
