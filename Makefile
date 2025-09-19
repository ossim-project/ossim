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

yq := $(PREFIX)bin/yq

.PHONY: install-bpf-deps
install-bpf-deps:
	sudo apt-get update && sudo apt-get install -y \
		libbpf-dev build-essential cmake \
		clang llvm pkg-config libelf-dev \
		protobuf-compiler libseccomp-dev

.PHONY: install-deps
install-deps:
	sudo apt-get update && sudo apt-get install -y \
		build-essential git qemu-system-x86 \
		libglib2.0-dev libfdt-dev libpixman-1-dev \
		zlib1g-dev ninja-build bear \
		build-essential libncurses-dev bison flex \
  		libssl-dev libelf-dev bc dwarves \
		guestfish cloud-image-utils \
		libfuse3-dev libcap-ng-dev \
		lld \
		meson ninja-build \
		xsltproc libxslt1-dev libgnutls28-dev \
		python3-docutils libjson-c-dev \
		libslirp-dev
	mkdir -p `dirname $(yq)` && \
	wget -qO $(yq) https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 && \
	chmod a+x $(yq)

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
