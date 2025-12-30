ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(abspath $(O))
else
  KBUILD_OUTPUT := $(CURDIR)/build
endif

# Always redirect to out-of-tree build directory unless we're already there
ifneq ($(KBUILD_OUTPUT), $(CURDIR))
# Only redirect if ROOT_SRC_DIR is not set (i.e., we're not already redirected)
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

# SRC_DIR is set by the top-level make call for out-of-source builds
export SRC_DIR ?= $(CURDIR)
export OBJ_DIR := $(CURDIR)

# Compiler flags
export CXXFLAGS := -std=c++17 -Wall -Wextra -I$(SRC_DIR) -I$(OBJ_DIR)
export THREAD_DEPS := -lpthread

# Manually specify grpc/protobuf flags because pkg-config takes a long time
GRPC_CXXFLAGS ?= $(shell pkg-config --cflags grpc++ protobuf)
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)

TARGET := ossimd

all: $(OBJ_DIR)/$(TARGET)


PROTO_DEPS := $(OBJ_DIR)/ossim_rpc.pb.o $(OBJ_DIR)/ossim_rpc.grpc.pb.o

# Proto file location in shared lib directory
PROTO_DIR ?= $(SRC_DIR)/../lib/ossim
PROTO_FILE := $(PROTO_DIR)/ossim_rpc.proto

LIB_DIR := $(SRC_DIR)/../lib
LIB_DEPS := $(shell ls $(LIB_DIR)/ossim/*.h)
LIB_INCLUDES := -I$(LIB_DIR)

# Kernel UAPI headers
KERNEL_UAPI_INCLUDES ?=

GRPC_DEPS := $(OBJ_DIR)/ossim_rpc.pb.h $(OBJ_DIR)/ossim_rpc.grpc.pb.h
GRPC_INCLUDES := -I$(OBJ_DIR)

DEPS := $(LIB_DEPS) $(GRPC_DEPS)
INCLUDES := $(LIB_INCLUDES) $(KERNEL_UAPI_INCLUDES) $(GRPC_INCLUDES)

# Generate protobuf and gRPC source files
$(OBJ_DIR)/ossim_rpc.pb.cc $(OBJ_DIR)/ossim_rpc.pb.h: $(PROTO_FILE)
	@echo "Generating protobuf code: $< -> $(OBJ_DIR)/"
	@mkdir -p $(OBJ_DIR)
	protoc --cpp_out=$(OBJ_DIR) --proto_path=$(PROTO_DIR) ossim_rpc.proto

$(OBJ_DIR)/ossim_rpc.grpc.pb.cc $(OBJ_DIR)/ossim_rpc.grpc.pb.h: $(PROTO_FILE)
	@echo "Generating gRPC code: $< -> $(OBJ_DIR)/"
	@mkdir -p $(OBJ_DIR)
	protoc --grpc_out=$(OBJ_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` --proto_path=$(PROTO_DIR) ossim_rpc.proto

# Compile protobuf object
$(OBJ_DIR)/ossim_rpc.pb.o: $(OBJ_DIR)/ossim_rpc.pb.cc
	@echo "Compiling protobuf: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -c $< -o $@

# Compile gRPC object
$(OBJ_DIR)/ossim_rpc.grpc.pb.o: $(OBJ_DIR)/ossim_rpc.grpc.pb.cc
	@echo "Compiling gRPC: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) -c $< -o $@

# Compile main C++ file
$(OBJ_DIR)/main.o: $(SRC_DIR)/main.cpp $(DEPS)
	@echo "Compiling main: $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(GRPC_CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/$(TARGET): $(OBJ_DIR)/main.o $(PROTO_DEPS)
	@echo "Linking: $@"
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(THREAD_DEPS) $(GRPC_LDFLAGS)

run: $(OBJ_DIR)/$(TARGET)
	$<

clean:
	rm -rf $(wildcard $(OBJ_DIR)/*)

.PHONY: all run clean

endif  # End of ifeq ($(skip-makefile),)
