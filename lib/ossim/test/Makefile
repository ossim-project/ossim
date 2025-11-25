# Makefile for libossim test programs
#
# Test programs link against the gRPC-based libossim library

# Support out-of-tree builds
SRC_DIR ?= $(CURDIR)

CC := gcc
CXX := g++
CFLAGS := -std=gnu11 -Wall -O2
CXXFLAGS := -std=c++17 -Wall -O2
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
LDFLAGS := -l:libossim.a $(GRPC_LDFLAGS) -lgrpc++_reflection -lstdc++ -lm

TEST_PROGS := test_vcpu_loop

all: $(TEST_PROGS)

test_vcpu_loop: $(SRC_DIR)/test_vcpu_loop.c
	@echo "Building vCPU loop test: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean
