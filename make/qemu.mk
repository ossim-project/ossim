qemu_d := $(d)qemu/
qemu_b := $(b)qemu/
myqemu := $(PREFIX)bin/qemu-system-x86_64


.PHONY: myqemu
myqemu: $(myqemu)

$(myqemu): build-qemu

.PHONY: configure-qemu
configure-qemu: install-libossim
	@mkdir -p $(qemu_b)
	cd $(qemu_b) && $(abspath $(qemu_d)configure) \
		--prefix=$(abspath $(PREFIX)) \
		--target-list=x86_64-softmmu \
		--enable-numa \
		--enable-slirp \
		--enable-kvm \
		--enable-ossim


.PHONY: build-qemu
build-qemu: install-libossim
	$(MAKE) -C $(qemu_b) -j`nproc`
	$(MAKE) -C $(qemu_b) install

.PHONY: clean-qemu
clean-qemu:
	rm -rf $(qemu_b)
