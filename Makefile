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

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

-include $(DEP)

clean:
	rm -rf build $(TARGET)
