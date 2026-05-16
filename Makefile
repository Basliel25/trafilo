# trafilo — streaming event-handler framework in C
# Targets:
#   make            - static + shared lib + pkg-config (default)
#   make static     - build/libtrafilo.a
#   make shared     - build/libtrafilo.so.<VER> (+ SONAME + linker name symlinks)
#   make pc         - build/trafilo.pc
#   make test       - build and run unit tests
#   make install    - install lib/headers/pc to $(DESTDIR)$(PREFIX)
#   make uninstall  - remove installed files
#   make clean      - wipe build/ and *.o
#
# Layout under $(PREFIX) after install:
#   $(libdir)/libtrafilo.a
#   $(libdir)/libtrafilo.so          -> libtrafilo.so.$(SOVERSION)
#   $(libdir)/libtrafilo.so.$(SOVERSION) -> libtrafilo.so.$(VERSION)
#   $(libdir)/libtrafilo.so.$(VERSION)
#   $(libdir)/pkgconfig/trafilo.pc
#   $(includedir)/trafilo.h

VERSION   := 0.1.0
SOVERSION := 0

# Install Layout
PREFIX     ?= /usr/local
DESTDIR    ?=
libdir     ?= $(PREFIX)/lib
includedir ?= $(PREFIX)/include
pcdir      ?= $(libdir)/pkgconfig

# Toolcahins
CC      := gcc
AR      := ar
CFLAGS  := -std=gnu11 -Wall -Wextra -Wpedantic -O2 -g \
           -Iinclude -Isrc/headers
PICFLAGS := -fPIC
LDFLAGS := -pthread

# Soruces
LIB_SRCS := $(wildcard src/*.c)
LIB_OBJS_STATIC := $(LIB_SRCS:.c=.o)
LIB_OBJS_SHARED := $(LIB_SRCS:.c=.lo)

BUILDDIR := build

STATIC_LIB := $(BUILDDIR)/libtrafilo.a
SHARED_REAL    := $(BUILDDIR)/libtrafilo.so.$(VERSION)
SHARED_SONAME  := $(BUILDDIR)/libtrafilo.so.$(SOVERSION)
SHARED_LINKER  := $(BUILDDIR)/libtrafilo.so
PC_FILE        := $(BUILDDIR)/trafilo.pc

# Tests
UNITY_SRC := tests/unity/src/unity.c
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/test_%)

.PHONY: all static shared pc test install uninstall clean
all: static shared pc

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Two different object tres to avoid conflict
# Non-PIC objects
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# PIC objects 
%.lo: %.c
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

# Static Library
static: $(STATIC_LIB)

$(STATIC_LIB): $(LIB_OBJS_STATIC) | $(BUILDDIR)
	$(AR) rcs $@ $(LIB_OBJS_STATIC)

# Shared names and symlinks
# Real file:   libtrafilo.so.0.1.0
shared: $(SHARED_LINKER)

$(SHARED_REAL): $(LIB_OBJS_SHARED) | $(BUILDDIR)
	$(CC) -shared -Wl,-soname,libtrafilo.so.$(SOVERSION) \
	    -o $@ $(LIB_OBJS_SHARED) $(LDFLAGS)

$(SHARED_SONAME): $(SHARED_REAL)
	ln -sf libtrafilo.so.$(VERSION) $@

$(SHARED_LINKER): $(SHARED_SONAME)
	ln -sf libtrafilo.so.$(SOVERSION) $@

# Package config
pc: $(PC_FILE)

$(PC_FILE): | $(BUILDDIR)
	@echo "prefix=$(PREFIX)"                  >  $@
	@echo "exec_prefix=\$${prefix}"           >> $@
	@echo "libdir=$(libdir)"                  >> $@
	@echo "includedir=$(includedir)"          >> $@
	@echo ""                                  >> $@
	@echo "Name: trafilo"                     >> $@
	@echo "Description: Streaming event-handler framework in C" >> $@
	@echo "URL: https://github.com/yourname/trafilo" >> $@
	@echo "Version: $(VERSION)"               >> $@
	@echo "Libs: -L\$${libdir} -ltrafilo"     >> $@
	@echo "Libs.private: -pthread"            >> $@
	@echo "Cflags: -I\$${includedir}"         >> $@

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "Running $$t"; ./$$t; done

$(BUILDDIR)/test_%: tests/test_%.c $(STATIC_LIB) $(UNITY_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) -Itests/unity/src $< $(UNITY_SRC) $(STATIC_LIB) -o $@ $(LDFLAGS)

# Install directives
INSTALL      ?= install
INSTALL_DATA ?= $(INSTALL) -m 644
INSTALL_DIR  ?= $(INSTALL) -d -m 755

install: all
	$(INSTALL_DIR) $(DESTDIR)$(libdir)
	$(INSTALL_DIR) $(DESTDIR)$(includedir)
	$(INSTALL_DIR) $(DESTDIR)$(pcdir)
	# headers
	$(INSTALL_DATA) include/trafilo.h $(DESTDIR)$(includedir)/trafilo.h
	# static objects
	$(INSTALL_DATA) $(STATIC_LIB) $(DESTDIR)$(libdir)/libtrafilo.a
	$(INSTALL_DATA) $(SHARED_REAL) $(DESTDIR)$(libdir)/libtrafilo.so.$(VERSION)
	ln -sf libtrafilo.so.$(VERSION)   $(DESTDIR)$(libdir)/libtrafilo.so.$(SOVERSION)
	ln -sf libtrafilo.so.$(SOVERSION) $(DESTDIR)$(libdir)/libtrafilo.so
	# pkg-config
	$(INSTALL_DATA) $(PC_FILE) $(DESTDIR)$(pcdir)/trafilo.pc
	@echo ""
	@echo "trafilo $(VERSION) installed under $(DESTDIR)$(PREFIX)"
	@echo "  export PKG_CONFIG_PATH=$(pcdir):\$$PKG_CONFIG_PATH"

uninstall:
	rm -f $(DESTDIR)$(includedir)/trafilo.h
	rm -f $(DESTDIR)$(libdir)/libtrafilo.a
	rm -f $(DESTDIR)$(libdir)/libtrafilo.so
	rm -f $(DESTDIR)$(libdir)/libtrafilo.so.$(SOVERSION)
	rm -f $(DESTDIR)$(libdir)/libtrafilo.so.$(VERSION)
	rm -f $(DESTDIR)$(pcdir)/trafilo.pc

# Claen
clean:
	rm -rf $(BUILDDIR)
	rm -f src/*.o src/*.lo
