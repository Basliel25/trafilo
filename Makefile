CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Wpedantic -g -Iinclude -Isrc/headers
LDFLAGS := -pthread

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)
BIN  := build/trafilo

$(BIN): $(OBJS)
	@mkdir -p build
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf build src/*.o

.PHONY: clean
