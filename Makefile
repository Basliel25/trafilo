CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wpedantic -g -Iinclude -Isrc/headers
LDFLAGS := -pthread

# Core framework sources 
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)
BIN  := build/trafilo

# Unity test harness
UNITY_SRC := tests/unity/src/unity.c
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=build/test_%)

EXAMPLE_SRCS := $(wildcard examples/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:examples/%.c=build/%)

build/hiveparser: LDLIBS := -lncurses

.PHONY: all examples test clean run-hiveparser

all: $(BIN) examples

examples: $(EXAMPLE_BINS)

$(BIN): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

build/%: examples/%.c $(OBJS)
	@mkdir -p build
	$(CC) -std=gnu11 -Wall -Wextra -g -Iinclude \
	    examples/$*.c $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "Running $$t"; ./$$t; done

build/test_%: tests/test_%.c $(SRCS) $(UNITY_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests/unity/src $^ -o $@ $(LDFLAGS)

run-hiveparser: build/hiveparser
	./build/hiveparser

clean:
	rm -rf build src/*.o
