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

export BPF_CLANG ?= clang
export BPFTOOL ?= bpftool

ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
	TARGET_ARCH := x86
else ifeq ($(ARCH),aarch64)
	TARGET_ARCH := arm64
else ifeq ($(ARCH),s390x)
	TARGET_ARCH := s390
else
	TARGET_ARCH := $(ARCH)
endif

ENDIAN ?= $(shell printf '\1\0' | od -An -t x2 | awk '{print ($$1=="0001"?"little":"big")}')

LIBBPF_CFLAGS := $(shell pkg-config --cflags libbpf)
LIBBPF_LIBS := $(shell pkg-config --libs libbpf)

export BPF_CFLAGS := -g -O2 -Wall -Wno-compare-distinct-pointer-types \
	-D__TARGET_ARCH_$(TARGET_ARCH) -mcpu=v3 -m$(ENDIAN)-endian

# SRC_DIR is set by the top-level make call for out-of-source builds
export SRC_DIR ?= $(CURDIR)
export OBJ_DIR := $(CURDIR)

SCX_SRC_DIR ?= $(OBJ_DIR)/scx
SCX_VERSION := 1.0.16

export BPF_INCLUDES := -I$(SCX_SRC_DIR)/scheds/include \
	-I$(SCX_SRC_DIR)/scheds/include/bpf-compat \
	-I$(SCX_SRC_DIR)/scheds/include/lib \
	-I$(SCX_SRC_DIR)/scheds/vmlinux \
	-I$(SCX_SRC_DIR)/scheds/vmlinux/arch/$(TARGET_ARCH) \
	$(LIBBPF_CFLAGS)

export CFLAGS := -std=gnu11 -I$(SCX_SRC_DIR)/scheds/include -I$(SCX_SRC_DIR)/scheds/vmlinux -I$(SRC_DIR) -I$(OBJ_DIR) $(LIBBPF_CFLAGS)
export CXXFLAGS := -std=c++17 -fpermissive -Wno-write-strings -I$(SCX_SRC_DIR)/scheds/include -I$(SCX_SRC_DIR)/scheds/vmlinux -I$(SRC_DIR) -I$(OBJ_DIR) $(LIBBPF_CFLAGS)

export LIBBPF_DEPS := $(LIBBPF_LIBS) -lelf -lz -lzstd
export THREAD_DEPS := -lpthread

# Manually specify grpc/protobuf flags because pkg-config takes a long time
GRPC_CXXFLAGS ?= $(shell pkg-config --cflags grpc++ protobuf)
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)

TARGET := ossimd

all: $(OBJ_DIR)/$(TARGET)


PROTO_DEPS := $(OBJ_DIR)/ossim_rpc.pb.o $(OBJ_DIR)/ossim_rpc.grpc.pb.o

# Proto file location in shared lib directory
PROTO_DIR := $(SRC_DIR)/../lib/ossim
PROTO_FILE := $(PROTO_DIR)/ossim_rpc.proto

LIB_DIR := $(SRC_DIR)/../lib
LIB_DEPS := $(shell ls $(LIB_DIR)/ossim/*.h)
LIB_INCLUDES := -I$(LIB_DIR)
SCX_DEPS := $(OBJ_DIR)/scx_ossim/scx_ossim.bpf.skel.h $(SRC_DIR)/scx_ossim/interface.h
SCX_INCLUDES := -I$(OBJ_DIR)
GRPC_DEPS := $(OBJ_DIR)/ossim_rpc.pb.h $(OBJ_DIR)/ossim_rpc.grpc.pb.h
GRPC_INCLUDES := -I$(OBJ_DIR)

DEPS := $(SCX_DEPS) $(LIB_DEPS) $(GRPC_DEPS)
INCLUDES := $(LIB_INCLUDES) $(SCX_INCLUDES) $(GRPC_INCLUDES)

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
	$(CXX) $^ -o $@ $(LIBBPF_DEPS) $(THREAD_DEPS) $(GRPC_LDFLAGS)

$(OBJ_DIR)/scx_ossim/scx_ossim.bpf.o: $(SRC_DIR)/scx_ossim/scx_ossim.bpf.c $(SRC_DIR)/scx_ossim/interface.h $(SCX_SRC_DIR)/
	@echo "Compiling BPF: $< -> $@"
	@mkdir -p $(dir $@)
	$(BPF_CLANG) $(BPF_CFLAGS) -target bpf $(BPF_INCLUDES) -c $< -o $@

$(OBJ_DIR)/scx_ossim/scx_ossim.bpf.skel.h: $(OBJ_DIR)/scx_ossim/scx_ossim.bpf.o
	@echo "Generating skeleton: $@"
	@mkdir -p $(dir $@)
	$(BPFTOOL) gen skeleton $< name $(basename $(basename $(notdir $<))) > $@

.PRECIOUS: $(SCX_SRC_DIR)/
$(SCX_SRC_DIR)/:
	rm -rf $(dir $@)
	git clone https://github.com/sched-ext/scx.git --branch v$(SCX_VERSION) $(dir $@)

# sudo-rs (default in Ubuntu 25.04) does not support `sudo -E`. We use sudo -i to source `/etc/profile` for environment variables.
run: $(OBJ_DIR)/$(TARGET)
	sudo -i $<

clean:
	rm -rf $(filter-out $(SCX_SRC_DIR),$(wildcard $(OBJ_DIR)/*))

clean-all:
	rm -rf $(wildcard $(OBJ_DIR)/*)

.PHONY: all run clean clean-all

endif  # End of ifeq ($(skip-makefile),)
