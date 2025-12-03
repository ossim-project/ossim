all:
	@echo Hello Ossim
.PHONY: all

OSSIM_PREFIX ?= install/
OSSIM_BUILD ?= build/
OSSIM_OUTPUT ?= out/

PREFIX := $(abspath $(OSSIM_PREFIX))
BUILD := $(OSSIM_BUILD)
OUTPUT := $(OSSIM_OUTPUT)

include make/include.mk

CLANG_FORMAT ?= clang-format
CLANG_FORMAT_STYLE ?= $(project_root).clang-format

clean: 
	rm -rf $(CLEAN_ALL)
.PHONY: clean

include make/lib.mk
include make/qemu.mk
include make/libvirt.mk
include make/linux.mk
include make/ossimd.mk
include make/ossimctl.mk

