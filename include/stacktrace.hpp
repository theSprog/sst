// stacktrace.hpp - header-only stack backtrace tool
// Requirements:
// - Only uses symbol table (no DWARF)
// - Handles PIE vs non-PIE automatically
// - Resolves symbols in all loaded modules (including dlopen'd ones)

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <elf.h>
#include <link.h>

namespace stacktrace {

struct Symbol {
    uintptr_t addr;
    std::string name;
};

struct ResolvedFrame {
    size_t index = 0;
    uintptr_t abs_addr = 0;
    std::string function;
    std::string module;
    uintptr_t offset = 0;
    bool has_symbol = false;

    ResolvedFrame() : index(0), abs_addr(0), function(), module(), offset(0), has_symbol(false) {}

    std::string to_string() const {
        std::ostringstream oss;
        oss << "[" << index << "] ";
        if (has_symbol) {
            oss << function << "+0x" << std::hex << offset << std::dec;
        } else {
            oss << "(no symbol)";
        }
        oss << " in " << module;
        oss << " (" << reinterpret_cast<void*>(abs_addr) << ")";
        return oss.str();
    }
};

namespace {
inline bool is_pie_binary(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        return false;
    }
    close(fd);
    return ehdr.e_type == ET_DYN;
}

inline size_t estimate_exe_size(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return 0;
    }
    void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return 0;
    }
    Elf64_Ehdr* ehdr = reinterpret_cast<Elf64_Ehdr*>(data);
    Elf64_Phdr* phdrs = reinterpret_cast<Elf64_Phdr*>(reinterpret_cast<char*>(data) + ehdr->e_phoff);
    uintptr_t end = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_LOAD) {
            uintptr_t seg_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            if (seg_end > end) end = seg_end;
        }
    }
    munmap(data, st.st_size);
    close(fd);
    return end;
}

inline std::string demangle(const char* name) {
    int status = 0;
    char* realname = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    std::string result = (status == 0 && realname) ? realname : name;
    free(realname);
    return result;
}

inline std::vector<Symbol> load_symbols(const char* path, uintptr_t base) {
    std::vector<Symbol> syms;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return syms;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return syms;
    }

    void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return syms;
    }

    auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(data);
    auto* shdrs = reinterpret_cast<Elf64_Shdr*>(reinterpret_cast<char*>(data) + ehdr->e_shoff);

    Elf64_Sym* symtab = nullptr;
    const char* strtab = nullptr;
    size_t nsyms = 0;

    auto try_section = [&](uint32_t target_type) -> bool {
        for (int i = 0; i < ehdr->e_shnum; ++i) {
            if (shdrs[i].sh_type == target_type) {
                symtab = reinterpret_cast<Elf64_Sym*>(reinterpret_cast<char*>(data) + shdrs[i].sh_offset);
                nsyms = shdrs[i].sh_size / sizeof(Elf64_Sym);
                strtab = reinterpret_cast<const char*>(data) + shdrs[shdrs[i].sh_link].sh_offset;
                return true;
            }
        }
        return false;
    };

    if (! try_section(SHT_SYMTAB)) {
        try_section(SHT_DYNSYM); // fallback only if symtab not found
    }

    if (symtab && strtab) {
        bool is_pie = is_pie_binary(path);
        for (size_t i = 0; i < nsyms; ++i) {
            const auto& s = symtab[i];
            if (ELF64_ST_TYPE(s.st_info) == STT_FUNC && s.st_value > 0) {
                uintptr_t symbol_addr = is_pie ? (s.st_value + base) : s.st_value;
                syms.push_back({symbol_addr, strtab + s.st_name});
            }
        }
    }

    munmap(data, st.st_size);
    close(fd);

    std::sort(syms.begin(), syms.end(), [](const Symbol& a, const Symbol& b) { return a.addr < b.addr; });

    return syms;
}

inline const Symbol* find_symbol(uintptr_t addr, const std::vector<Symbol>& symbols) {
    auto it =
        std::upper_bound(symbols.begin(), symbols.end(), addr, [](uintptr_t a, const Symbol& b) { return a < b.addr; });
    if (it == symbols.begin()) return nullptr;
    --it;
    return &(*it);
}

struct Module {
    std::string path;
    uintptr_t base;
    size_t size;
    std::vector<Symbol> symbols;
    bool symbols_loaded = false;

    Module(const std::string& path, uintptr_t base, size_t size,
           std::vector<Symbol> symbols = {}, bool loaded = false)
        : path(path), base(base), size(size),
          symbols(std::move(symbols)), symbols_loaded(loaded) {}

    void ensure_symbols_loaded() {
        if (! symbols_loaded) {
            symbols = load_symbols(path.c_str(), base);
            symbols_loaded = true;
        }
    }

    bool contains(uintptr_t addr) const {
        return addr >= base && addr < base + size;
    }
};

class ModuleManager {
  public:
    ModuleManager() : initialized_(false), modules_{} {}

    static ModuleManager& instance() {
        static ModuleManager m;
        return m;
    }

    const std::vector<Module>& all_modules() {
        if (! initialized_) {
            load_modules();
        }
        return modules_;
    }

    void load_modules() {
        modules_.clear();
        initialized_ = true;

        const char* exe = "/proc/self/exe";
        bool is_pie = is_pie_binary(exe);

        if (is_pie) {
            // PIE 程序，使用 dl_iterate_phdr() 加载全部模块
            dl_iterate_phdr(
                [](struct dl_phdr_info* info, size_t, void* data) {
                    auto& mods = *reinterpret_cast<std::vector<Module>*>(data);
                    std::string path = (info->dlpi_name && *info->dlpi_name) ? info->dlpi_name : "/proc/self/exe";
                    uintptr_t min_addr = static_cast<uintptr_t>(-1), max_addr = 0;
                    for (int i = 0; i < info->dlpi_phnum; ++i) {
                        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
                            uintptr_t seg_start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
                            uintptr_t seg_end = seg_start + info->dlpi_phdr[i].p_memsz;
                            if (seg_start < min_addr) min_addr = seg_start;
                            if (seg_end > max_addr) max_addr = seg_end;
                        }
                    }
                    size_t module_size = max_addr - min_addr;
                    mods.push_back(Module{path, info->dlpi_addr, module_size, {}, false});
                    return 0;
                },
                &modules_);
        } else {
            // no-pie：手动构造 main 模块
            uintptr_t base = 0x400000;
            size_t size = estimate_exe_size(exe);
            modules_.push_back(Module{exe, base, size, {}, false});
        }
    }

    void clear() {
        initialized_ = false;
        modules_.clear();
    }

  private:
    bool initialized_;
    std::vector<Module> modules_;
};

} // namespace

class Stacktrace {
    static constexpr size_t kMaxFrames = 32;

  public:
    static Stacktrace capture(size_t max_frames = kMaxFrames) {
        Stacktrace st;
        if (max_frames > st.frames_.size()) {
            max_frames = st.frames_.size(); // 确保不越界
        }
        st.size_ = ::backtrace(st.frames_.data(), static_cast<int>(max_frames));
        return st;
    }

    static ResolvedFrame resolve_with_modules(void* address, const std::vector<Module>& modules) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(address);
        ResolvedFrame f;
        f.abs_addr = addr;
        for (const auto& m : modules) {
            if (m.contains(addr)) {
                const_cast<Module&>(m).ensure_symbols_loaded();
                auto* sym = find_symbol(addr, m.symbols);
                if (sym) {
                    f.has_symbol = true;
                    f.offset = addr - sym->addr;
                    f.function = demangle(sym->name.c_str());
                    f.module = m.path;
                } else {
                    f.module = m.path;
                }
                break;
            }
        }
        return f;
    }

    static ResolvedFrame resolve(void* address) {
        return resolve_with_modules(address, ModuleManager::instance().all_modules());
    }

    std::vector<ResolvedFrame> get_frames() const {
        const auto& mods = ModuleManager::instance().all_modules();
        std::vector<ResolvedFrame> out;
        for (size_t i = 0; i < size_; ++i) {
            auto f = resolve_with_modules(frames_[i], mods);
            f.index = i;
            out.push_back(std::move(f));
        }
        return out;
    }

    void print(std::ostream& os = std::cout) const {
        for (const auto& f : get_frames()) {
            os << f.to_string() << "\n";
        }
    }

  private:
    std::array<void*, kMaxFrames> frames_{};
    size_t size_ = 0;
};

} // namespace internal
