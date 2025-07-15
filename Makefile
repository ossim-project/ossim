# output directory 
O ?= out/
B ?= build/

O := $(if $(filter %/,$(O)),$(O),$(O)/)
B := $(if $(filter %/,$(B)),$(B),$(B)/)

d := ./
o := $(O)
b := $(B)
project_root := $(d)

current_makefile := $(firstword $(MAKEFILE_LIST))
makefile_stack := $(current_makefile)

define update_current_makefile
	$(eval current_makefile := $(firstword $(makefile_stack)))
	$(eval d := $(dir $(current_makefile)))
	$(eval o := $(subst /.,,$(O)$(d)))
	$(eval b := $(subst /.,,$(B)$(d)))
endef

ALL_ALL :=
CLEAN_ALL := 
EXTERNAL_CLEAN_ALL := 
INPUT_TAR_ALL :=

define include_rules
	$(eval makefile_stack := $(1) $(makefile_stack))
	$(eval $(call update_current_makefile))
	$(eval include $(1))
	$(eval makefile_stack := $(wordlist 2, $(words $(makefile_stack)),$(makefile_stack)))
	$(eval $(call update_current_makefile))
endef

.PHONY: help
help:
	@echo "Hello Ossim!"

.PHONY: all
all: $(ALL_ALL)

.PHONY: clean
clean: 
	rm -rf $(CLEAN_ALL)

.PHONY: install-dependencies
install-dependencies:
	sudo apt-get update && sudo apt-get install -y \
		libglib2.0-dev libfdt-dev libpixman-1-dev \
		zlib1g-dev ninja-build

qemu_d := $(d)qemu/
qemu_b := $(b)qemu/
qemu_o := $(o)qemu/
qemu := $(qemu_o)bin/qemu-system-x86_64

$(qemu) := configure-qemu build-qemu

.PHONY: configure-qemu
configure-qemu:
	@mkdir -p $(qemu_b)
	@mkdir -p $(qemu_o)
	cd $(qemu_b) && $(realpath $(qemu_d)configure) \
		--prefix=$(realpath $(qemu_o)) \
		--target-list=x86_64-softmmu \
		--enable-numa \
		--enable-slirp \
		--enable-kvm

$(qemu_b)Makefile: configure-qemu
	@mkdir -p $(qemu_b)
	cd $(qemu_b) && $(realpath $(qemu_d)configure) \
		--prefix=$(realpath $(qemu_o)) \
		--target-list=x86_64-softmmu

.PHONY: build-qemu
build-qemu: $(qemu_b)Makefile
	$(MAKE) -C $(qemu_b) -j`nproc`
	$(MAKE) -C $(qemu_b) install

.PHONY: clean-qemu
clean-qemu:
	rm -rf $(qemu_b) $(qemu_o)

$(eval $(call include_rules,$(d)images/rules.mk))
