// sst.hpp - header-only stack backtrace tool
// Requirements:
// - Only uses symbol table (no DWARF)
// - Handles PIE vs non-PIE automatically
// - Handles static vs dynamic linking automatically
// - Resolves symbols in all loaded modules (including dlopen'd ones)

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits.h>
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

struct RawFrame {
    uintptr_t abs_addr = 0;
    uintptr_t offset = 0;
    std::string module;
    bool has_symbol = false;

    RawFrame() : abs_addr(0), offset(0), module(), has_symbol(false) {}
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
        oss << " (" << reinterpret_cast<void*>(abs_addr) << ")\n";
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

// 获取主程序基地址
inline uintptr_t get_exe_base() {
    char exe_path[PATH_MAX] = {};
    ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) return 0;
    exe_path[len] = '\0';

    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream iss(line);
        std::string addr_range, perms, offset, dev, inode, path;

        // eg. 00400000-00401000 r--p 00000000 08:10 1487 /path/to/test
        if (! (iss >> addr_range >> perms >> offset >> dev >> inode)) continue;
        std::getline(iss, path);                    // 路径可能带空格, 因此单独获取
        path.erase(0, path.find_first_not_of(' ')); // trim leading spaces
        if (path != exe_path) continue;             // 只找主程序

        size_t dash = addr_range.find('-');
        if (dash == std::string::npos) continue;

        std::string start_addr_str = addr_range.substr(0, dash);
        uintptr_t start_addr = std::stoull(start_addr_str, nullptr, 16);
        return start_addr;
    }
    return 0;
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

static std::string get_real_exe_path() {
    std::ifstream ifs("/proc/self/cmdline", std::ios::in | std::ios::binary);
    if (! ifs) return "/proc/self/exe"; // fallback

    std::string path;
    std::getline(ifs, path, '\0'); // 读取第一个 null-terminated 参数
    return path.empty() ? "/proc/self/exe" : path;
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
        // 如果没有找到 symtab 则退化到寻找 dynsym (例如对于 libc.so.6 就是只有 dynsym)
        try_section(SHT_DYNSYM);
    }

    if (symtab && strtab) {
        bool is_pie = is_pie_binary(path);
        for (size_t i = 0; i < nsyms; ++i) {
            const auto& s = symtab[i];
            if (ELF64_ST_TYPE(s.st_info) == STT_FUNC && s.st_value > 0) {
                // 如果是非 pie, 则符号地址就是绝对地址
                // 如果是 ET_DYN, st_value 表示 相对地址, 必须加 base 才能得出真实的地址
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

    Module(const std::string& path, uintptr_t base, size_t size, std::vector<Symbol> symbols = {}, bool loaded = false)
        : path(path), base(base), size(size), symbols(std::move(symbols)), symbols_loaded(loaded) {}

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

    std::vector<Module>& all_modules() {
        if (! initialized_) {
            load_modules();
        }
        return modules_;
    }

    void load_modules() {
        modules_.clear();
        initialized_ = true;

        dl_iterate_phdr(
            [](struct dl_phdr_info* info, size_t, void* data) {
                auto& mods = *reinterpret_cast<std::vector<Module>*>(data);

                // 获取模块路径, "" 代表主程序自身
                std::string path = (info->dlpi_name && *info->dlpi_name) ? info->dlpi_name : "/proc/self/exe";

                uintptr_t min_addr = static_cast<uintptr_t>(-1), max_addr = 0;
                for (int i = 0; i < info->dlpi_phnum; ++i) {
                    /*
                        有些函数存在于非可执行段中（少数编译器行为），
                        加 (phdr[i].p_flags & PF_X) != 0) 过滤条件反而漏了, 
                        所以此处不能用 p_flags 过滤
                    */
                    if (info->dlpi_phdr[i].p_type == PT_LOAD) {
                        /*
                            no-pie 时 info->dlpi_addr == 0, p_vaddr (+0) 就是段映射的实际虚拟地址
                            pie 时 info->dlpi_addr != 0, p_vaddr + dlpi_addr 才是段映射的实际虚拟地址
                        */
                        uintptr_t seg_start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
                        uintptr_t seg_end = seg_start + info->dlpi_phdr[i].p_memsz;
                        if (seg_start < min_addr) min_addr = seg_start;
                        if (seg_end > max_addr) max_addr = seg_end;
                    }
                }

                uintptr_t base = info->dlpi_addr;

                /*
                修正 -static/-no-pie 主程序 dlpi_addr == 0 的情况, 
                不能直接将 dlpi_addr 用于 base, 因为后续的 contains 会误判
                */
                if ((path == "/proc/self/exe" || path.empty()) && base == 0) {
                    base = get_exe_base(); // -static/-no-pie 程序加载基地址
                }

                if (min_addr < max_addr) { // 若 min_addr > max_addr 则说明本 module 不存在可 load 的段
                    size_t size = max_addr - min_addr;
                    mods.emplace_back(path, base, size);
                }

                return 0;
            },
            &modules_);
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

  private:
    static ResolvedFrame resolve_with_modules(void* address, std::vector<Module>& modules) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(address);
        ResolvedFrame f;
        f.abs_addr = addr;
        for (auto& m : modules) {
            if (m.contains(addr)) {
                m.ensure_symbols_loaded();
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

    static RawFrame resolve_to_raw_with_modules(void* address, std::vector<Module>& modules) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(address);
        RawFrame f;
        f.abs_addr = addr;
        for (auto& m : modules) {
            if (m.contains(addr)) {
                f.has_symbol = true;
                if (is_pie_binary(m.path.c_str())) {
                    f.offset = addr - m.base;
                } else {
                    f.offset = addr;
                }
                f.module = m.path;
                
                if (f.module == "/proc/self/exe") {
                    f.module = get_real_exe_path();
                }
                break;
            }
        }

        return f;
    }

  public:
    static Stacktrace capture(size_t max_frames = kMaxFrames) {
        Stacktrace st;
        if (max_frames > st.frames_.size()) {
            max_frames = st.frames_.size(); // 确保不越界
        }
        st.size_ = ::backtrace(st.frames_.data(), static_cast<int>(max_frames));
        return st;
    }

    static ResolvedFrame resolve(void* address) {
        auto& mods = ModuleManager::instance().all_modules();
        return resolve_with_modules(address, mods);
    }

    static RawFrame resolve_to_raw(void* address) {
        auto& mods = ModuleManager::instance().all_modules();
        return resolve_to_raw_with_modules(address, mods);
    }

    std::vector<RawFrame> get_raw_frames() const {
        auto& mods = ModuleManager::instance().all_modules();
        std::vector<RawFrame> out;
        for (size_t i = 0; i < size_; ++i) {
            auto rf = resolve_to_raw_with_modules(frames_[i], mods);
            out.push_back(std::move(rf));
        }
        return out;
    }

    std::vector<ResolvedFrame> get_frames() const {
        auto& mods = ModuleManager::instance().all_modules();
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
            os << f.to_string();
        }
    }

  private:
    std::array<void*, kMaxFrames> frames_{};
    size_t size_ = 0;
};

} // namespace stacktrace
