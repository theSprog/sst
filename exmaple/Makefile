CXX := g++
CXXFLAGS := -std=c++11 -g -Wall -Wextra -Weffc++ -O0 -fno-exceptions -fno-inline
CXXFLAGS += -Werror=uninitialized \
    -Werror=return-type \
    -Wconversion \
    -Wsign-compare \
    -Werror=unused-result \
    -Werror=suggest-override \
    -Wzero-as-null-pointer-constant \
    -Wmissing-declarations \
    -Wold-style-cast \
    -Werror=vla \
    -Wnon-virtual-dtor
CXXFLAGS += -Wno-missing-declarations -Wno-unused-parameter 


SRC := $(wildcard *.cpp)
BUILD := build
OBJ := $(BUILD)/libfoo.o

.PHONY: all clean
all: $(BUILD)/libfoo.a $(BUILD)/libfoo.so \
     $(BUILD)/static \
     $(BUILD)/pie \
     $(BUILD)/pie_shared \
     $(BUILD)/pie_dlopen \
     $(BUILD)/nopie_static \
     $(BUILD)/nopie_shared \
     $(BUILD)/nopie_shared_static \
     $(BUILD)/nopie_dlopen \
     $(BUILD)/nopie_dlopen_static \
	 $(BUILD)/target_pid 

# 创建 build 目录
$(BUILD):
	mkdir -p $(BUILD)

# 编译 libfoo
$(BUILD)/libfoo.o: libfoo.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(BUILD)/libfoo.so: $(BUILD)/libfoo.o
	$(CXX) -shared -o $@ $<

$(BUILD)/libfoo.a: $(BUILD)/libfoo.o
	ar rcs $@ $<

# 编译静态链接可执行
$(BUILD)/static: static.cpp $(BUILD)
	$(CXX) $(CXXFLAGS) -static $< -o $@

# 编译 PIE 可执行
$(BUILD)/pie: pie.cpp $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIE -pie $< -o $@

# PIE + 共享库
$(BUILD)/pie_shared: pie_shared.cpp $(BUILD)/libfoo.so | $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIE -pie $< -L$(BUILD) -lfoo -o $@

$(BUILD)/pie_dlopen: pie_dlopen.cpp $(BUILD)/libfoo.so | $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIE -pie $< -ldl -o $@

# No-PIE 各种方式
$(BUILD)/nopie_static: nopie_static.cpp $(BUILD)/libfoo.a | $(BUILD)
	$(CXX) $(CXXFLAGS) -no-pie -static $< -L$(BUILD) -o $@

$(BUILD)/nopie_shared: nopie_shared.cpp $(BUILD)/libfoo.so | $(BUILD)
	$(CXX) $(CXXFLAGS) -no-pie $< -L$(BUILD) -lfoo -o $@

$(BUILD)/nopie_shared_static: nopie_shared_static.cpp $(BUILD)/libfoo.a | $(BUILD)
	$(CXX) $(CXXFLAGS) -no-pie $< -L$(BUILD) -lfoo -o $@

$(BUILD)/nopie_dlopen: nopie_dlopen.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -no-pie $< -ldl -o $@

$(BUILD)/nopie_dlopen_static: nopie_dlopen_static.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -no-pie -static $< -ldl -o $@

$(BUILD)/target_pid: target_pid.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)
