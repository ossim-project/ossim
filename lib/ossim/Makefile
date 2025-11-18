# Makefile for libossim - gRPC-based control library
#
# This library provides C API for communicating with scx_ossim scheduler daemon

CXX := g++
CXXFLAGS := -std=c++17 -Wall -O2 -fPIC -I. -I..
GRPC_CXXFLAGS ?= $(shell pkg-config --cflags grpc++ protobuf)
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf) 
GRPC_LDFLAGS := $(GRPC_LDFLAGS) -lgrpc++_reflection

# Proto file
PROTO_FILE := ossim_rpc.proto

# Generated files
PROTO_SRCS := ossim_rpc.pb.cc ossim_rpc.grpc.pb.cc
PROTO_HDRS := ossim_rpc.pb.h ossim_rpc.grpc.pb.h
PROTO_OBJS := ossim_rpc.pb.o ossim_rpc.grpc.pb.o

# Library sources
LIB_SRCS := ossim_ctl.cpp
LIB_OBJS := ossim_ctl.o

# Library targets
LIB_STATIC := libossim.a
LIB_SHARED := libossim.so

all: $(LIB_STATIC) $(LIB_SHARED)

# Generate protobuf code
%.pb.cc %.pb.h: $(PROTO_FILE)
	@echo "Generating protobuf code: $<"
	protoc --cpp_out=. --proto_path=. $<

# Generate gRPC code
%.grpc.pb.cc %.grpc.pb.h: $(PROTO_FILE)
	@echo "Generating gRPC code: $<"
	protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` --proto_path=. $<

# Compile protobuf objects
ossim_rpc.pb.o: ossim_rpc.pb.cc
	@echo "Compiling protobuf: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -c $< -o $@

ossim_rpc.grpc.pb.o: ossim_rpc.grpc.pb.cc
	@echo "Compiling gRPC: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -c $< -o $@

# Compile library
ossim_ctl.o: ossim_ctl.cpp $(PROTO_HDRS) ossim_ctl.h types.h config.h
	@echo "Compiling library: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -c $< -o $@

# Build static library
$(LIB_STATIC): $(LIB_OBJS) $(PROTO_OBJS)
	@echo "Building static library: $@"
	ar rcs $@ $^

# Build shared library
$(LIB_SHARED): $(LIB_OBJS) $(PROTO_OBJS)
	@echo "Building shared library: $@"
	$(CXX) -shared -o $@ $^ $(GRPC_LDFLAGS)

clean:
	rm -f $(LIB_OBJS) $(PROTO_OBJS) $(PROTO_SRCS) $(PROTO_HDRS) $(LIB_STATIC) $(LIB_SHARED)

.PHONY: all clean
