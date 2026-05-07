CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wpedantic -g -Iinclude -Isrc/headers
LDFLAGS := -pthread

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)
BIN  := build/trafilo

UNITY_SRC := tests/unity/src/unity.c
TEST_SRCS  := $(wildcard tests/test_*.c)
TEST_BINS  := $(TEST_SRCS:tests/test_%.c=build/test_%)

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "Running $$t"; ./$$t; done

build/test_%: tests/test_%.c src/%.c $(UNITY_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests/unity/src $^ -o $@ $(LDFLAGS)

clean:
	rm -rf build src/*.o
