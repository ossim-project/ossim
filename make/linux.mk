LINUX_TARGET := vmlinux

linux_d := $(d)linux/
linux_b := $(b)linux/
linux_o := $(o)linux/

linux_vmlinux := $(linux_o)vmlinux
$(linux_vmlinux): $(linux_b)vmlinux
	@mkdir -p $(@D)
	cp $< $@

# $(linux_b).config:
# 	$(MAKE) configure-linux-qemu

$(linux_b).config:
	$(MAKE) configure-linux-local

$(linux_b)vmlinux:
	$(MAKE) build-linux

.PHONY: configure-linux-local
configure-linux-local:
	@mkdir -p $(linux_b)
	cp /boot/config-`uname -r` $(linux_b).config
	$(linux_d)scripts/config \
		--file $(linux_b).config \
		--disable CONFIG_MODULES \
		--disable CONFIG_MODULE_SIG \
		--disable CONFIG_MODULE_SIG_ALL \
		--set-str CONFIG_SYSTEM_TRUSTED_KEYS "" \
		--set-str CONFIG_SYSTEM_REVOCATION_KEYS "" \
		--set-str CONFIG_MODULE_SIG_KEY ""
	$(linux_d)scripts/config \
		--file $(linux_b).config \
        --disable CONFIG_WIRELESS \
        --disable CONFIG_WLAN \
        --disable CONFIG_CFG80211 \
        --disable CONFIG_MAC80211 \
        --disable CONFIG_IWLWIFI \
        --disable CONFIG_BT \
        --disable CONFIG_IEEE802154 \
        --disable CONFIG_NET_VENDOR_MELLANOX \
        --disable CONFIG_MELLANOX_PLATFORM \
        --disable CONFIG_INFINIBAND \
        --disable CONFIG_COMEDI \
        --disable CONFIG_IIO \
        --disable CONFIG_I2C \
        --disable CONFIG_SPI \
        --disable CONFIG_GPIO \
        --disable CONFIG_HID \
        --disable CONFIG_MEDIA_SUPPORT \
        --disable CONFIG_SOUND \
        --disable CONFIG_INPUT_MOUSE \
        --disable CONFIG_INPUT_JOYSTICK \
        --disable CONFIG_INPUT_TABLET \
        --disable CONFIG_INPUT_TOUCHSCREEN \
        --disable CONFIG_INPUT_MISC \
        --disable CONFIG_HID_SUPPORT \
        --disable CONFIG_HID \
        --disable CONFIG_DRM \
        --disable CONFIG_DRM_AMDGPU \
        --disable CONFIG_DRM_VIRTIO_GPU \
        --disable CONFIG_FB
	$(MAKE) -C $(linux_d) O=$(abspath $(linux_b)) olddefconfig

.PHONY: build-linux
build-linux: $(linux_b).config
	$(MAKE) LD=ld.lld -C $(linux_b) -j`nproc` $(LINUX_TARGET)

.PHONY: clean-linux
clean-linux:
	rm -rf $(linux_b) $(linux_o)
