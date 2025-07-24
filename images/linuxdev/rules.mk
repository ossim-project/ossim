LINUXDEV_DIMG_ISO_URL := https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
LINUXDEV_DIMG_CKSUM_URL := https://cloud-images.ubuntu.com/noble/current/SHA256SUMS
LINUXDEV_DIMG_DISK_SIZE := 40G

LINUXDEV_KERNEL_CMDLINE := earlyprintk=ttyS0 console=ttyS0 root=/dev/vda1 rw

linuxdev_dimg := $(o)linuxdev/disk.qcow2

.PHONY: linuxdev-dimg
linuxdev-dimg: $(linuxdev_dimg)

$(linuxdev_dimg): $(o)base/disk.qcow2
	@mkdir -p $(@D)
	$(qemu_img) create -f qcow2 -F qcow2 -b $(realpath --relative-to=$@ $<) $@

$(o)base/disk.qcow2: $(linux_d)Makefile $(b)seed.raw $(d)install.sh $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=$(LINUXDEV_DIMG_DISK_SIZE)" \
	-var "iso_url=$(LINUXDEV_DIMG_ISO_URL)" \
	-var "iso_cksum_url=$(LINUXDEV_DIMG_CKSUM_URL)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "seedimg=$(word 2,$^)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "input_dir=$(linux_d)" \
	-var "install_script=$(word 3,$^)" \
	$(base_hcl)

$(b)seed.raw: $(d)user-data $(b)meta-data
	@mkdir -p $(@D)
	cloud-localds $@ $^

$(b)meta-data:
	@mkdir -p $(@D)
	tee $@ < /dev/null > /dev/null

linuxdev_initrd := $(o)initrd.img
linuxdev_config := $(o)config

$(linuxdev_initrd): $(o)base/disk.qcow2
	@mkdir -p $(@D)
	sudo $(virt_copy_out) -a $< /root/output/initrd.img $(@D)
	sudo chown $(shell id -u):$(shell id -g) $@
	touch $@

$(linuxdev_config): $(o)base/disk.qcow2
	@mkdir -p $(@D)
	sudo $(virt_copy_out) -a $< /root/output/config $(@D)
	sudo chown $(shell id -u):$(shell id -g) $@
	touch $@

.PHONY: qemu-linuxdev
qemu-linuxdev: $(linuxdev_dimg) $(linux_vmlinux)
	$(qemu) -machine q35,accel=kvm -cpu host -smp 8 -m 32G \
	-kernel $(linux_vmlinux) \
	-append "$(LINUXDEV_KERNEL_CMDLINE)" \
	-drive file=$(linuxdev_dimg),media=disk,format=qcow2,if=virtio,index=0 \
	-netdev user,id=user-net \
	-device virtio-net-pci,netdev=user-net \
	-boot c \
	-display none -serial mon:stdio

.PHONY: configure-linux
configure-linux: $(linuxdev_config)
	@mkdir -p $(linux_b)
	cp $< $(linux_b).config
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
