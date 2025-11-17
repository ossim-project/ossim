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

export LIBBPF_DEPS := $(LIBBPF_LIBS) -lelf -lz -lzstd
export THREAD_DEPS := -lpthread

# Scheduler lists for convenience targets

TARGET := ossimd

all: $(OBJ_DIR)/$(TARGET)

DEPS := $(OBJ_DIR)/scx_ossim/scx_ossim.bpf.skel.h $(SRC_DIR)/scx_ossim/interface.h

$(OBJ_DIR)/$(TARGET): $(SRC_DIR)/main.c $(DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBBPF_DEPS) $(THREAD_DEPS) -I$(OBJ_DIR)

$(OBJ_DIR)/scx_ossim/scx_ossim.bpf.o: $(SRC_DIR)/scx_ossim/scx_ossim.bpf.c $(SRC_DIR)/scx_ossim/interface.h $(SCX_SRC_DIR)/
	@echo "Compiling BPF: $< -> $@"
	@mkdir -p $(dir $@)
	$(BPF_CLANG) $(BPF_CFLAGS) -target bpf $(BPF_INCLUDES) -c $< -o $@

$(OBJ_DIR)/scx_ossim/scx_ossim.bpf.skel.h: $(OBJ_DIR)/scx_ossim/scx_ossim.bpf.o
	@echo "Generating skeleton: $@"
	@mkdir -p $(dir $@)
	$(BPFTOOL) gen skeleton $< name $(basename $(basename $(notdir $<))) > $@

.PRECIOUS: $(SCX_SRC_DIR)/ $(SCX_SRC_DIR)/Makefile.md
$(SCX_SRC_DIR)/Makefile.md:
	rm -rf $(dir $@)
	git clone https://github.com/sched-ext/scx.git --branch v$(SCX_VERSION) $(dir $@)

run: $(OBJ_DIR)/$(TARGET)
	sudo $<

clean:
	rm -f $(OBJ_DIR)/*.bpf.o $(OBJ_DIR)/*.bpf.skel.h $(OBJ_DIR)/*.l1o $(OBJ_DIR)/*.l2o $(OBJ_DIR)/*.l3o $(OBJ_DIR)/$(TARGET)
	rm -rf $(OBJ_DIR)/scx/

.PHONY: all clean run

endif  # End of ifeq ($(skip-makefile),)
