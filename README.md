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
## 🔧 Quick Start (C++)

You only need a single header:

```cpp
#include "sst.hpp"

int main() {
    stacktrace::Stacktrace st = stacktrace::Stacktrace::capture();
    st.print();  // Print current stack frames
}
```

---

### Raw Frame Structure

> 💡 `RawFrame` is a lower-level structure designed to work well with tools like `addr2line`, which allow symbolic address resolution via commands like:
>
> `addr2line -e /lib/libc.so.6 0x1234`

If you want to integrate with `addr2line`, or build custom backtrace analysis tools, you can use `resolve_to_raw()` or `get_raw_frames()` to retrieve absolute addresses, module offsets, and file paths:

```cpp
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
               f.module_name.c_str());
    }

    // You can also resolve a single address
    RawFrame one = Stacktrace::resolve_to_raw((void*)main);
}
```

---

## 🌐 C API Usage

You can build `libsst.a` or `libsst.so` to use the library from C projects or foreign language bindings:

```c
#include "sst.h"

int main() {
    sst_backtrace bt;
    sst_capture(&bt);
    sst_print_stdout(&bt);
    sst_print(&bt, stderr); // Print to a custom file stream

    // Extract raw frame info (can be used with addr2line)
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

    sst_free_raw_frames(raw, bt.size); // Free allocated module strings
    return 0;
}
```

> 💡 To ensure the full module path is preserved for tools like `addr2line`, the `module` field must be a dynamically allocated `char*`, not a fixed-size array. Fixed-size buffers may truncate long paths and break tooling. All `sst_raw_frame.module` values are allocated via `strdup()` internally, and must be manually freed to avoid memory leaks.

---

## 🛠️ Build Instructions

```bash
cd src && make       # Build both static and shared libraries

# If you want to run tests
cd test && make      # Build C test binaries
```

---

## 🧼 Memory Management (C)

All `sst_raw_frame.module` strings returned by `sst_resolve_to_raw()` or `sst_resolve_raw_batch()` must be manually freed:

```c
sst_raw_frame frame;
sst_resolve_to_raw(ptr, &frame);
// Use frame.module ...
sst_free_raw_frames(&frame, 1);
```

For batch allocations, use `sst_free_raw_frames` to release each internal `char*`. This function does **not** free the array itself — you are responsible for that:

```c
sst_raw_frame* arr = malloc(sizeof(sst_raw_frame) * count);
sst_resolve_raw_batch(addrs, count, arr);
// Use arr[i].module ...
sst_free_raw_frames(arr, count);
free(arr); // Optional: free the array body itself
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
