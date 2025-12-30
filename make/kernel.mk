# kernel.mk - Build and test custom kernel with virtme-ng

kernel_d := $(d)kernel/
kernel_b := $(b)kernel/
kernel_o := $(o)kernel/

kernel_vmlinux := $(kernel_b)vmlinux
kernel_bzImage := $(kernel_b)arch/x86/boot/bzImage

# virtme-ng configuration
VNG ?= vng
VNG_OPTS ?=
VNG_RW ?= 0

# Configure kernel from host config with disabled problematic options
.PHONY: configure-kernel
configure-kernel:
	@mkdir -p $(kernel_b)
	cp /boot/config-`uname -r` $(kernel_b).config
	$(kernel_d)scripts/config \
		--file $(kernel_b).config \
		--set-str CONFIG_LOCALVERSION "-ossim" \
		--enable CONFIG_OSSIM \
		--disable CONFIG_LOCALVERSION_AUTO \
		--disable CONFIG_DEBUG_INFO_BTF \
		--disable CONFIG_MODULE_SIG \
		--disable CONFIG_MODULE_SIG_ALL \
		--set-str CONFIG_SYSTEM_TRUSTED_KEYS "" \
		--set-str CONFIG_SYSTEM_REVOCATION_KEYS "" \
		--set-str CONFIG_MODULE_SIG_KEY ""
	$(kernel_d)scripts/config \
		--file $(kernel_b).config \
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
		--disable CONFIG_DRM \
		--disable CONFIG_DRM_AMDGPU \
		--disable CONFIG_DRM_VIRTIO_GPU \
		--disable CONFIG_FB
# 	$(kernel_d)scripts/config \
# 		--file $(kernel_b).config \
# 		--enable CONFIG_VIRTIO \
# 		--enable CONFIG_VIRTIO_PCI \
# 		--enable CONFIG_VIRTIO_MMIO \
# 		--enable CONFIG_NET_9P \
# 		--enable CONFIG_NET_9P_VIRTIO \
# 		--enable CONFIG_9P_FS \
# 		--enable CONFIG_9P_FS_POSIX_ACL \
# 		--enable CONFIG_VIRTIO_NET \
# 		--enable CONFIG_VIRTIO_CONSOLE \
# 		--enable CONFIG_VIRTIO_BLK
	$(MAKE) -C $(kernel_d) O=$(abspath $(kernel_b)) olddefconfig

# Configure kernel with virtme-ng defaults (minimal config for fast builds)
.PHONY: configure-kernel-vng
configure-kernel-vng:
	@mkdir -p $(kernel_b)
	cd $(kernel_d) && $(VNG) --kconfig O=$(abspath $(kernel_b))
	$(kernel_d)scripts/config \
		--file $(kernel_b).config \
		--enable CONFIG_OSSIM
	$(MAKE) -C $(kernel_d) O=$(abspath $(kernel_b)) olddefconfig

# Build kernel
.PHONY: build-kernel
build-kernel: $(kernel_b).config
	$(MAKE) LD=ld.lld -C $(kernel_b)

# Build kernel modules only
.PHONY: build-kernel-modules
build-kernel-modules: $(kernel_b).config
	$(MAKE) LD=ld.lld -C $(kernel_b) modules

# Install kernel to host system
.PHONY: install-kernel
install-kernel: $(kernel_bzImage)
	$(SUDO) $(MAKE) -C $(kernel_b) modules_install install

# Clean kernel build
.PHONY: clean-kernel
clean-kernel:
	rm -rf $(kernel_b) $(kernel_o)

# Boot kernel with virtme-ng using host filesystem (read-only by default)
.PHONY: vng-kernel
vng-kernel: $(kernel_bzImage)
ifeq ($(VNG_RW),1)
	$(VNG) --run $(abspath $(kernel_b)) --rw $(VNG_OPTS)
else
	$(VNG) --run $(abspath $(kernel_b)) $(VNG_OPTS)
endif

# Boot kernel with virtme-ng in read-write mode
.PHONY: vng-kernel-rw
vng-kernel-rw: $(kernel_bzImage)
	$(VNG) --run $(abspath $(kernel_b)) --rw $(VNG_OPTS)

# Boot kernel with virtme-ng and run a specific command
# Usage: make vng-kernel-run VNG_CMD="uname -a"
VNG_CMD ?= uname -a
.PHONY: vng-kernel-run
vng-kernel-run: $(kernel_bzImage)
	$(VNG) --run $(abspath $(kernel_b)) --exec "$(VNG_CMD)"

# Quick rebuild and test cycle
.PHONY: test-kernel
test-kernel: build-kernel vng-kernel

# Persistent vng instance with SSH access via vsock (no TCP port needed)
# VNG_VSOCK_CID: vsock context ID for SSH (must be unique per VM, avoids TCP port conflicts)
VNG_VSOCK_CID ?= 2025
VNG_MEM ?= 8G
VNG_CPUS ?= 4
vng_b := $(b)vng/
VNG_PIDFILE := $(vng_b)vng.pid
VNG_LOG := $(vng_b)vng.log

# Start vng as a background instance with SSH access via vsock
.PHONY: vng-start
vng-start: $(kernel_bzImage)
	@mkdir -p $(vng_b)
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		echo "vng already running (pid=$$(cat $(VNG_PIDFILE)))"; \
		echo "Use 'make vng-ssh' to connect or 'make vng-stop' to stop"; \
	else \
		echo "Starting vng with SSH via vsock (cid=$(VNG_VSOCK_CID))..."; \
		nohup $(VNG) --run $(abspath $(kernel_b)) --rw \
			--memory $(VNG_MEM) --cpus $(VNG_CPUS) \
			--ssh $(VNG_VSOCK_CID) $(VNG_OPTS) \
			> $(VNG_LOG) 2>&1 & \
		echo $$! > $(VNG_PIDFILE); \
		sleep 5; \
		if kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
			echo "vng started (pid=$$(cat $(VNG_PIDFILE))). Use 'make vng-ssh' to connect."; \
		else \
			echo "vng failed to start. Check $(VNG_LOG)"; \
			rm -f $(VNG_PIDFILE); \
		fi \
	fi

# SSH into the running vng instance (uses vng's built-in ssh-client via vsock)
.PHONY: vng-ssh
vng-ssh:
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		$(VNG) --ssh-client $(VNG_VSOCK_CID); \
	else \
		echo "No vng instance running. Use 'make vng-start' first."; \
	fi

# Run a command in the running vng instance via SSH
# Usage: make vng-run VNG_CMD="uname -a"
.PHONY: vng-run
vng-run:
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		$(VNG) --ssh-client $(VNG_VSOCK_CID) --remote-cmd "$(VNG_CMD)"; \
	else \
		echo "No vng instance running. Use 'make vng-start' first."; \
	fi

# Run a command in vng (uses fresh instance, no persistent connection needed)
# Usage: make vng-exec VNG_CMD="uname -a"
.PHONY: vng-exec
vng-exec: $(kernel_bzImage)
	$(VNG) --run $(abspath $(kernel_b)) --rw --exec "$(VNG_CMD)"

# Stop the running vng instance
.PHONY: vng-stop
vng-stop:
	@if [ -f $(VNG_PIDFILE) ]; then \
		PID=$$(cat $(VNG_PIDFILE)); \
		if kill -0 $$PID 2>/dev/null; then \
			echo "Stopping vng (pid=$$PID)..."; \
			kill $$PID; \
			sleep 1; \
			kill -9 $$PID 2>/dev/null || true; \
			rm -f $(VNG_PIDFILE); \
			echo "vng stopped."; \
		else \
			echo "vng not running (stale pidfile)."; \
			rm -f $(VNG_PIDFILE); \
		fi \
	else \
		echo "No vng instance running."; \
	fi

# Check vng status
.PHONY: vng-status
vng-status:
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		echo "vng running (pid=$$(cat $(VNG_PIDFILE)), vsock cid=$(VNG_VSOCK_CID))"; \
	else \
		echo "vng not running"; \
	fi

# View vng log
.PHONY: vng-log
vng-log:
	@if [ -f $(kernel_b).vng.log ]; then \
		tail -50 $(kernel_b).vng.log; \
	else \
		echo "No vng log found."; \
	fi

# Dependency: ensure bzImage exists
$(kernel_bzImage): build-kernel

$(kernel_b).config:
	$(MAKE) configure-kernel
