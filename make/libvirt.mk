libvirt_d := $(d)libvirt/
libvirt_b := $(b)libvirt/

configure-libvirt:
	@echo "Configuring libvirt..."
	@mkdir -p $(libvirt_b)
	meson setup $(libvirt_b) $(libvirt_d) \
		--prefix=$(PREFIX) \
		--localstatedir=$(PREFIX)var \
 		-Drunstatedir=$(PREFIX)run \
		-Ddriver_qemu=enabled \
		-Ddocs=disabled
.PHONY: configure-libvirt

install-libvirt: $(libvirt_b)build.ninja
	@echo "Installing libvirt..."
	ninja -C $(libvirt_b)
	ninja -C $(libvirt_b) install
.PHONY: install-libvirt

clean-libvirt:
	rm -rf $(libvirt_b)
.PHONY: install-libvirt

$(libvirt_b)build.ninja:
	$(MAKE) configure-libvirt
