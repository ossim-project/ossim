ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(abspath $(O))
else
  KBUILD_OUTPUT := $(CURDIR)/build
endif

ifeq ($(SCX_SRC_DIR),)
$(error Please set SCX_SRC_DIR)
endif

SCX_SRC_DIR := $(abspath $(SCX_SRC_DIR))

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
export BPF_INCLUDES := -I$(SCX_SRC_DIR)/scheds/include \
	-I$(SCX_SRC_DIR)/scheds/include/bpf-compat \
	-I$(SCX_SRC_DIR)/scheds/include/lib \
	-I$(SCX_SRC_DIR)/scheds/vmlinux \
	-I$(SCX_SRC_DIR)/scheds/vmlinux/arch/$(TARGET_ARCH) \
	$(LIBBPF_CFLAGS)

export OBJ_DIR := $(CURDIR)

export CFLAGS := -std=gnu11 -I$(SCX_SRC_DIR)/scheds/include -I$(SCX_SRC_DIR)/scheds/vmlinux -I$(SRC_DIR) -I$(OBJ_DIR) $(LIBBPF_CFLAGS)

export LIBBPF_DEPS := $(LIBBPF_LIBS) -lelf -lz -lzstd
export THREAD_DEPS := -lpthread

# Scheduler lists for convenience targets
SCHEDS := scx_ossim

all: scheds

# Build regular schedulers (no library dependency) - from source to output dir
$(addprefix $(OBJ_DIR)/,$(SCHEDS)): $(OBJ_DIR)/%: $(SRC_DIR)/%.c $(OBJ_DIR)/%.bpf.skel.h
	@echo "Building scheduler: $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBBPF_DEPS) $(THREAD_DEPS)

$(OBJ_DIR)/%.bpf.o: $(SRC_DIR)/%.bpf.c
	@echo "Compiling BPF: $< -> $@"
	@mkdir -p $(dir $@)
	$(BPF_CLANG) $(BPF_CFLAGS) -target bpf $(BPF_INCLUDES) -c $< -o $@

$(OBJ_DIR)/%.bpf.skel.h: $(OBJ_DIR)/%.bpf.o
	@echo "Generating skeleton: $@"
	@mkdir -p $(dir $@)
	$(BPFTOOL) gen skeleton $< name $(basename $(basename $(notdir $<))) > $@

scheds: $(addprefix $(OBJ_DIR)/,$(SCHEDS))

run-%: $(OBJ_DIR)/%
	sudo $<

clean:
	rm -f *.bpf.o *.bpf.skel.h *.l1o *.l2o *.l3o scx_simple scx_qmap scx_central scx_userland scx_nest scx_flatcg scx_pair scx_prev scx_sdt
	rm -f $(OBJ_DIR)/*.bpf.o $(OBJ_DIR)/*.bpf.skel.h $(OBJ_DIR)/*.l1o $(OBJ_DIR)/*.l2o $(OBJ_DIR)/*.l3o $(ALL_SCHEDS)

.PHONY: all scheds clean $(SCHEDS) run-%

endif  # End of ifeq ($(skip-makefile),)
