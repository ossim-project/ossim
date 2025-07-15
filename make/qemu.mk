qemu_d := $(d)qemu/
qemu_b := $(b)qemu/
qemu_o := $(o)qemu/
myqemu := $(qemu_o)bin/qemu-system-x86_64

$(myqemu) := configure-qemu build-qemu

.PHONY: configure-qemu
configure-qemu:
	@mkdir -p $(qemu_b)
	@mkdir -p $(qemu_o)
	cd $(qemu_b) && $(abspath $(qemu_d)configure) \
		--prefix=$(abspath $(qemu_o)) \
		--target-list=x86_64-softmmu \
		--enable-numa \
		--enable-slirp \
		--enable-kvm

$(qemu_b)Makefile: configure-qemu
	@mkdir -p $(qemu_b)
	cd $(qemu_b) && $(abspath $(qemu_d)configure) \
		--prefix=$(realpath $(qemu_o)) \
		--target-list=x86_64-softmmu

.PHONY: build-qemu
build-qemu: $(qemu_b)Makefile
	$(MAKE) -C $(qemu_b) -j`nproc`
	$(MAKE) -C $(qemu_b) install

.PHONY: clean-qemu
clean-qemu:
	rm -rf $(qemu_b) $(qemu_o)
