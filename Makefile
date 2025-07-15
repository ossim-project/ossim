all:
	@echo Hello Ossim
.PHONY: all

OSSIM_PREFIX ?= install/
OSSIM_BUILD ?= build/
OSSIM_OUTPUT ?= out/

PREFIX := $(OSSIM_PREFIX)
BUILD := $(OSSIM_BUILD)
OUTPUT := $(OSSIM_OUTPUT)

include make/include.mk

.PHONY: install-dependencies
install-dependencies:
	sudo apt-get update && sudo apt-get install -y \
		git qemu-system-x86 \
		libglib2.0-dev libfdt-dev libpixman-1-dev \
		zlib1g-dev ninja-build bear \
		build-essential libncurses-dev bison flex \
  		libssl-dev libelf-dev bc dwarves \
		guestfish cloud-image-utils \
		libfuse3-dev libcap-ng-dev \
		lld

clean: 
	rm -rf $(CLEAN_ALL)
.PHONY: clean

qemu := qemu-system-x86_64

include make/qemu.mk
include make/linux.mk

$(eval $(call include_rules,$(d)images/rules.mk))

