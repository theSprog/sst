SRC_DIR    := ../src
LIBDIR     := .
BINDIR     := .
SRC        := test_capi.c

# === 输出文件 ===
OUT_STATIC := $(BINDIR)/test_static
OUT_DYN    := $(BINDIR)/test_dyn

# === 静态和动态库文件 ===
LIB_STATIC := $(LIBDIR)/libsst.a
LIB_DYN    := $(LIBDIR)/libsst.so

SRC_BUILD_DIR   := $(SRC_DIR)/build
LIB_A_SRC   := $(SRC_BUILD_DIR)/libsst.a
LIB_SO_SRC  := $(SRC_BUILD_DIR)/libsst.so
LIB_A_DST   := libsst.a
LIB_SO_DST  := libsst.so

# === 编译配置 ===
CC         := gcc
CFLAGS     := -I$(SRC_DIR) -Wall -Wextra -g
LDFLAGS_D  := -L$(LIBDIR) -lsst
LDFLAGS_S  := -static -L$(LIBDIR) -lsst -lstdc++ -lm -lpthread

# === 规则 ===

all: $(LIB_A_DST) $(LIB_SO_DST) $(OUT_STATIC) $(OUT_DYN)

# === 构建 libsst.{a,so} ===
$(LIB_A_DST) $(LIB_SO_DST):
	$(MAKE) -C $(SRC_DIR)
	cp $(LIB_A_SRC) $(LIB_A_DST)
	cp $(LIB_SO_SRC) $(LIB_SO_DST)

$(OUT_STATIC): $(SRC) $(LIB_STATIC)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_S)

$(OUT_DYN): $(SRC) $(LIB_DYN)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_D)

clean:
	rm -f $(LIB_A_DST) $(LIB_SO_DST) $(OUT_STATIC) $(OUT_DYN)

.PHONY: all clean
