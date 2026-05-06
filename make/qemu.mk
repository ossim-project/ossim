qemu_d := $(d)qemu/
qemu_b := $(b)qemu/
myqemu := $(PREFIX)bin/qemu-system-x86_64


.PHONY: myqemu
myqemu: $(myqemu)

$(myqemu): build-qemu

configure-qemu: install-libossim update-qemu-kernel-headers
	@mkdir -p $(qemu_b)
	cd $(qemu_b) && $(abspath $(qemu_d)configure) \
		--prefix=$(abspath $(PREFIX)) \
		--target-list=x86_64-softmmu \
		--extra-cflags=-Wno-error=discarded-qualifiers \
		--enable-numa \
		--enable-slirp \
		--enable-kvm \
		--enable-ossim

update-qemu-kernel-headers:
	cp $(kernel_d)/include/uapi/linux/ossim.h $(qemu_d)/linux-headers/linux/ossim.h

.PHONY: update-qemu-kernel-headers

qemu: install-libossim
	$(MAKE) -C $(qemu_b) -j$(JOBS)

install-qemu: qemu
	$(MAKE) -C $(qemu_b) install

clean-qemu:
	rm -rf $(qemu_b)

.PHONY: configure-qemu qemu install-qemu clean-qemu
