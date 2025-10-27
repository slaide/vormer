CC = gcc
INCLUDES = -Isrc -Iinclude -Iexternal/lua
LDFLAGS = -lm
LIBS = -lvulkan

# Build configuration
BUILD_DIR = build
CONFIG ?= release

ifeq ($(CONFIG),debug)
    CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -O0 -g -DDEBUG
    BUILD_SUBDIR = $(BUILD_DIR)/debug
else
    CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -O2 -g
    BUILD_SUBDIR = $(BUILD_DIR)/release
endif

# Platform detection
UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
    CFLAGS += -DPLATFORM_LINUX
    PLATFORM_SRCS = $(wildcard src/platform/linux/*.c)
    ifdef USE_WAYLAND
        CFLAGS += -DUSE_WAYLAND
        LIBS += -lwayland-client
    else
        CFLAGS += -DUSE_X11
        LIBS += -lxcb
    endif
endif

ifeq ($(UNAME),Darwin)
    CFLAGS += -DPLATFORM_MACOS
    PLATFORM_SRCS = $(wildcard src/platform/macos/*.m)
    LDFLAGS += -framework Cocoa -framework QuartzCore -framework Metal
    LIBS += -L/usr/local/lib -lMoltenVK
endif

SRC_DIRS = src src/core src/graphics src/parsers src/math src/scene src/scripting

C_SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
C_OBJS = $(patsubst %.c,$(BUILD_SUBDIR)/%.o,$(C_SRCS))
PLATFORM_OBJS = $(patsubst %.c,$(BUILD_SUBDIR)/%.o,$(filter %.c,$(PLATFORM_SRCS))) $(patsubst %.m,$(BUILD_SUBDIR)/%.o,$(filter %.m,$(PLATFORM_SRCS)))
OBJS = $(C_OBJS) $(PLATFORM_OBJS)

LUA_DIR = external/lua
LUA_LIB = $(LUA_DIR)/liblua.a

TEAL_DIR = external/teal
TEAL_BIN = $(TEAL_DIR)/teal

TEAL_SRCS = $(wildcard resources/scripts/*.tl)
TEAL_LUAS = $(TEAL_SRCS:.tl=.lua)

TARGET = resources/vormer

all: $(LUA_LIB) $(TEAL_LUAS) $(TARGET)

$(TARGET): $(OBJS) $(LUA_LIB) | resources
	$(CC) -o $@ $(OBJS) $(LUA_LIB) $(LDFLAGS) $(LIBS)

$(BUILD_SUBDIR)/%.o: %.c | $(BUILD_SUBDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_SUBDIR)/%.o: %.m | $(BUILD_SUBDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_SUBDIR):
	@mkdir -p $@

$(LUA_LIB):
	$(MAKE) -C $(LUA_DIR) a

%.lua: %.tl
	$(TEAL_BIN) --emit-asm $<

run: $(TARGET)
	./$(TARGET)

resources:
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(TEAL_LUAS)
	$(MAKE) -C $(LUA_DIR) clean

.PHONY: all run clean
