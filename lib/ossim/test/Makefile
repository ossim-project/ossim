# Makefile for libossim test programs
#
# This Makefile directly compiles the library sources for easier development.
# No need to rebuild libossim.a/libossim.so for every change.

CC := gcc
CFLAGS := -std=gnu11 -Wall -O2 -I../..
LDFLAGS :=

# Library sources (in parent directory)
LIB_SRCS := ../ossim_ctl.c
LIB_DEPS := $(shell find .. -name '*.h')

TEST_PROGS := test_socket_client

all: $(TEST_PROGS)

test_socket_client: test_socket_client.c $(LIB_SRCS) $(LIB_DEPS)
	@echo "Building test client: $@"
	$(CC) $(CFLAGS) $< $(LIB_SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean
