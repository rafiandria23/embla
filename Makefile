CC := clang

CFLAGS := \
	-std=c17 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-g \
	-MMD \
	-MP

TARGET := build/embla

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	PLATFORM_SRC := src/platform/memory_limit_darwin.c
else
	PLATFORM_SRC := src/platform/memory_limit_linux.c
endif

SRC := $(wildcard src/*.c) src/platform/rlimit_common.c $(PLATFORM_SRC)
OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:.o=.d)

LIB_OBJ := $(filter-out build/main.o,$(OBJ))

TEST_SRC := $(wildcard tests/*.c)
TEST_BIN := $(TEST_SRC:tests/%.c=build/tests/%)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

build/tests/%: tests/%.c $(LIB_OBJ)
	mkdir -p build/tests
	$(CC) $(CFLAGS) -Iinclude $< $(LIB_OBJ) -o $@

test: $(TEST_BIN)
	@for t in $(TEST_BIN); do echo "=== running $$t ==="; $$t || exit 1; done
	@echo "all tests passed"

-include $(DEP)

clean:
	rm -rf build $(TARGET)
