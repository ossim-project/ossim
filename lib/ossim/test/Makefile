# Makefile for libossim test programs
#
# Test programs link against the gRPC-based libossim library

CC := gcc
CXX := g++
CFLAGS := -std=gnu11 -Wall -O2 -I../..
CXXFLAGS := -std=c++17 -Wall -O2 -I../..
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
LDFLAGS := -L.. -l:libossim.a $(GRPC_LDFLAGS) -lgrpc++_reflection -lstdc++ -lm

TEST_PROGS := test_libossim_cli test_vcpu_loop

all: $(TEST_PROGS)

test_libossim_cli: test_libossim_cli.c
	@echo "Building libossim CLI test: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test_vcpu_loop: test_vcpu_loop.c
	@echo "Building vCPU loop test: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(TEST_PROGS): lib

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean

lib:
	$(MAKE) -C ..

clean-lib:
	rm -f ../libossim.a

.PHONY: lib clean-lib
