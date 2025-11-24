# Makefile for libossim test programs
#
# Test programs link against the gRPC-based libossim library

# Support out-of-tree builds
SRC_DIR ?= $(CURDIR)
LIB_BUILD_DIR ?= $(SRC_DIR)/../build
LIB_SRC_DIR ?= $(SRC_DIR)/..

CC := gcc
CXX := g++
CFLAGS := -std=gnu11 -Wall -O2 -I$(LIB_SRC_DIR)/..
CXXFLAGS := -std=c++17 -Wall -O2 -I$(LIB_SRC_DIR)/..
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
LDFLAGS := -L$(LIB_BUILD_DIR) -l:libossim.a $(GRPC_LDFLAGS) -lgrpc++_reflection -lstdc++ -lm

TEST_PROGS := test_libossim_cli test_vcpu_loop

all: $(TEST_PROGS)

test_libossim_cli: $(SRC_DIR)/test_libossim_cli.c $(LIB_BUILD_DIR)/libossim.a
	@echo "Building libossim CLI test: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test_vcpu_loop: $(SRC_DIR)/test_vcpu_loop.c $(LIB_BUILD_DIR)/libossim.a
	@echo "Building vCPU loop test: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(LIB_BUILD_DIR)/libossim.a:
	@echo "Error: libossim.a not found at $(LIB_BUILD_DIR)"
	@echo "Please build libossim first with: make -C $(LIB_SRC_DIR)"
	@exit 1

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean
