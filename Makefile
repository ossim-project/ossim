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

yq := yq

clean: 
	rm -rf $(CLEAN_ALL)
.PHONY: clean

qemu := qemu-system-x86_64

include make/qemu.mk
include make/libvirt.mk
include make/linux.mk
include make/kmod.mk
include make/sched.mk

$(eval $(call include_rules,$(d)utils/rules.mk))
$(eval $(call include_rules,$(d)images/rules.mk))
