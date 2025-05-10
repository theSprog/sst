CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Weffc++ -O2 -g
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


SRC := sst.cpp
BUILD := build
OBJ := $(BUILD)/libsst.o

.PHONY: all clean

all: $(BUILD) $(BUILD)/libsst.a $(BUILD)/libsst.so
	rm -f $(OBJ)

# 创建 build 目录
$(BUILD):
	mkdir -p $(BUILD)

# 编译 lib
$(OBJ): sst.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(BUILD)/libsst.so: $(OBJ)
	$(CXX) -shared -o $@ $<

$(BUILD)/libsst.a: $(OBJ)
	ar rcs $@ $<

clean:
	rm -rf $(BUILD)
