# Makefile for libossim - gRPC-based control library
#
# This library provides C API for communicating with ossimd

ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(abspath $(O))
else
  KBUILD_OUTPUT := $(CURDIR)/build
endif

# Always redirect to out-of-tree build directory unless we're already there
ifneq ($(KBUILD_OUTPUT), $(CURDIR))
# Only redirect if SRC_DIR is not set (i.e., we're not already redirected)
ifeq ($(SRC_DIR),)

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out sub-make,$(MAKECMDGOALS)): sub-make
	@:

sub-make:
	@mkdir -p $(KBUILD_OUTPUT)
	@$(MAKE) --no-print-directory -C $(KBUILD_OUTPUT) -f $(CURDIR)/Makefile \
		SRC_DIR=$(abspath $(CURDIR)) \
		$(MAKECMDGOALS)

.PHONY: $(PHONY)

# Skip the rest of the makefile when redirecting
skip-makefile := 1
endif
endif

ifeq ($(skip-makefile),)

# Build configuration
CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2 -fPIC -I$(SRC_DIR)/..
GRPC_CXXFLAGS ?= $(shell pkg-config --cflags grpc++ protobuf)
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
GRPC_LDFLAGS := $(GRPC_LDFLAGS) -lgrpc++_reflection

# Directory configuration
export SRC_DIR ?= $(CURDIR)
export OBJ_DIR := $(CURDIR)
PREFIX ?= /usr/local

# Proto file
PROTO_FILE := $(SRC_DIR)/ossim_rpc.proto

# Generated files (in build directory)
PROTO_SRCS := $(OBJ_DIR)/ossim_rpc.pb.cc $(OBJ_DIR)/ossim_rpc.grpc.pb.cc
PROTO_HDRS := $(OBJ_DIR)/ossim_rpc.pb.h $(OBJ_DIR)/ossim_rpc.grpc.pb.h
PROTO_OBJS := $(OBJ_DIR)/ossim_rpc.pb.o $(OBJ_DIR)/ossim_rpc.grpc.pb.o

# Library sources (in build directory)
LIB_SRCS := $(SRC_DIR)/ossim_ctl.cpp
LIB_OBJS := $(OBJ_DIR)/ossim_ctl.o

# Library targets (in build directory)
LIB_STATIC := $(OBJ_DIR)/libossim.a
LIB_SHARED := $(OBJ_DIR)/libossim.so

# Public headers to install
PUBLIC_HEADERS := $(SRC_DIR)/ossim_ctl.h $(SRC_DIR)/types.h $(SRC_DIR)/config.h

all: $(LIB_STATIC) $(LIB_SHARED)

# Generate protobuf code
$(OBJ_DIR)/ossim_rpc.pb.cc $(OBJ_DIR)/ossim_rpc.pb.h: $(PROTO_FILE)
	@echo "Generating protobuf code: $< -> $(OBJ_DIR)/"
	protoc --cpp_out=$(OBJ_DIR) --proto_path=$(SRC_DIR) ossim_rpc.proto

# Generate gRPC code (depends on pb.h being generated first)
$(OBJ_DIR)/ossim_rpc.grpc.pb.cc $(OBJ_DIR)/ossim_rpc.grpc.pb.h: $(PROTO_FILE) $(OBJ_DIR)/ossim_rpc.pb.h
	@echo "Generating gRPC code: $< -> $(OBJ_DIR)/"
	protoc --grpc_out=$(OBJ_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` --proto_path=$(SRC_DIR) ossim_rpc.proto

# Compile protobuf objects
$(OBJ_DIR)/ossim_rpc.pb.o: $(OBJ_DIR)/ossim_rpc.pb.cc $(OBJ_DIR)/ossim_rpc.pb.h
	@echo "Compiling protobuf: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -I$(OBJ_DIR) -c $< -o $@

$(OBJ_DIR)/ossim_rpc.grpc.pb.o: $(OBJ_DIR)/ossim_rpc.grpc.pb.cc $(OBJ_DIR)/ossim_rpc.grpc.pb.h $(OBJ_DIR)/ossim_rpc.pb.h
	@echo "Compiling gRPC: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -I$(OBJ_DIR) -c $< -o $@

# Compile library
$(OBJ_DIR)/ossim_ctl.o: $(LIB_SRCS) $(PROTO_HDRS) $(SRC_DIR)/ossim_ctl.h $(SRC_DIR)/types.h $(SRC_DIR)/config.h
	@echo "Compiling library: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -I$(OBJ_DIR) -c $< -o $@

# Build static library
$(LIB_STATIC): $(LIB_OBJS) $(PROTO_OBJS)
	@echo "Building static library: $@"
	ar rcs $@ $^

# Build shared library
$(LIB_SHARED): $(LIB_OBJS) $(PROTO_OBJS)
	@echo "Building shared library: $@"
	$(CXX) -shared -o $@ $^ $(GRPC_LDFLAGS)

# Install target
install: $(LIB_STATIC) $(LIB_SHARED) $(PROTO_HDRS)
	@echo "Installing libraries to $(PREFIX)/lib"
	@mkdir -p $(PREFIX)/lib
	install -m 644 $(LIB_STATIC) $(PREFIX)/lib/
	install -m 755 $(LIB_SHARED) $(PREFIX)/lib/
	@echo "Installing headers to $(PREFIX)/include/ossim"
	@mkdir -p $(PREFIX)/include/ossim
	install -m 644 $(PUBLIC_HEADERS) $(PREFIX)/include/ossim/
	install -m 644 $(PROTO_HDRS) $(PREFIX)/include/ossim/

clean:
	rm -f $(LIB_OBJS) $(PROTO_OBJS) $(PROTO_SRCS) $(PROTO_HDRS) $(LIB_STATIC) $(LIB_SHARED)

clean-all:
	rm -rf $(wildcard $(OBJ_DIR)/*)

# Test targets
test-build: $(LIB_STATIC) $(LIB_SHARED)
	@echo "Building tests..."
	$(MAKE) -C $(SRC_DIR)/test SRC_DIR=$(SRC_DIR)/test LIB_BUILD_DIR=$(OBJ_DIR) LIB_SRC_DIR=$(SRC_DIR)

test-clean:
	$(MAKE) -C $(SRC_DIR)/test SRC_DIR=$(SRC_DIR)/test clean

.PHONY: all install clean clean-all test-build test-clean

endif  # End of ifeq ($(skip-makefile),)
