ifeq ($(OSSIM_PREFIX),)
$(error OSSIM_PREFIX is not set)
endif
ifeq ($(OSSIM_BUILD_DIR),)
$(error OSSIM_BUILD_DIR is not set)
endif
ifeq ($(OSSIM_OUT_DIR),)
$(error OSSIM_OUT_DIR is not set)
endif

PREFIX := $(abspath $(OSSIM_PREFIX))
BUILD := $(OSSIM_BUILD_DIR)
OUTPUT := $(OSSIM_OUT_DIR)

include make/include.mk

all:
	@echo Hello Ossim
.PHONY: all

CLANG_FORMAT ?= clang-format
CLANG_FORMAT_STYLE ?= $(project_root).clang-format

SUDO_ENV ?= LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PATH=$(PATH)
ifdef SUDO_PASS
SUDO ?= sh -c 'echo "$(SUDO_PASS)" | /usr/bin/sudo.ws -S $(SUDO_ENV) "$$@"' _
else
SUDO ?= /usr/bin/sudo.ws $(SUDO_ENV)
endif

clean: 
	rm -rf $(CLEAN_ALL)
.PHONY: clean

include make/lib.mk
include make/qemu.mk
include make/libvirt.mk
include make/linux.mk
include make/ossimd.mk
include make/ossimctl.mk
