中文版文档在[这里](./README_zh.md)

`sst(simple stack trace)` is a lightweight, zero-dependency, **header-only** C++ stacktrace library designed for minimal intrusion and high compatibility. It supports:

- ✅ **No DWARF Required**: Uses only the symbol table (`.symtab` / `.dynsym`), no reliance on DWARF debug info
- ✅ **PIE / No-PIE Support**: Automatically detects whether the main binary is PIE and handles address relocation properly
- ✅ **Static / Dynamic Linking**: Works with `-static`, `-no-pie`, `-pie`, and `-lxxx` builds out of the box
- ✅ **Dynamic Library Resolution**: Can resolve symbols from modules loaded via `dlopen()`
- ✅ **C API Export**: Provides a C-compatible API via `libsst.so` / `libsst.a` for integration with C or other languages

---



## 📁 Project Structure

```
.
├── include/
│   └── sst.hpp          # ✅ Core header-only file for direct C++ use
├── src/
│   ├── sst.cpp          # 🔁 C API implementation
│   └── sst.h            # 🔁 C API header (useful for Python FFI or other bindings)
├── exmaple/
│   └── *.cpp            # 📦 Example programs under various build configurations (PIE, no-PIE, static, shared, dlopen)
├── test/
│   ├── test_capi.c      # 🧪 Test program demonstrating the C API
└── README.md            # 📖 Project documentation

````

---



## 🔧 Getting Started (C++)

All you need is one header:

```cpp
#include "sst.hpp"

int main() {
    stacktrace::Stacktrace st = stacktrace::Stacktrace::capture();
    st.print();  // Print the current call stack
}
````

---



## 🌐 Using the C API

You can build `libsst.a` or `libsst.so` and use it from C or other languages:

```c
#include "sst.h"

int main() {
    sst_backtrace trace;
    sst_capture(&trace);
    sst_print_stdout(&bt);
    sst_print(&bt, stderr); // output to customizable file
    return 0;
}
```

Build instructions:

```bash
cd src && make       # Build the static and shared libraries

# if you want to test
cd test && make      # Compile the test using the C API
```

---



## 💡 Example Usage

Check out the `exmaple/` directory for sample programs covering different scenarios:

* `pie.cpp`, `nopie.cpp`, `static.cpp`: Test various main binary configurations
* `pie_dlopen.cpp`, `nopie_dlopen.cpp`: Test symbol resolution for `dlopen()`-loaded modules
* `*_shared*.cpp`: Test whether symbols from linked shared libraries are correctly resolved

---



## 🛠️ Technical Details

* Uses `dl_iterate_phdr()` to enumerate all loaded modules (including the main binary and shared libraries)
* Parses `.symtab` and `.dynsym` from ELF files directly
* Resolves symbol addresses as `dlpi_addr + st_value` for PIE binaries, or just `st_value` for no-PIE
* For static or no-PIE binaries (where `dlpi_addr == 0`), uses `/proc/self/maps` to determine the true base address

## 📬 Contact me
Feel free to submit an Issue or PR for contribution(If there are bugs, PLEASE TELL ME!😘)
