# kernel.mk - Build and test custom kernel with virtme-ng
LOCAL_KERNEL_CONFIG := /boot/config-$(shell uname -r)

kernel_d := $(d)kernel

local_kernel_b := $(b)kernel.local
vng_kernel_b := $(b)kernel.vng

# virtme-ng configuration
VNG ?= vng
VNG_OPTS ?=
VNG_RW ?= 0

# Configure kernel from host config with disabled problematic options
.PHONY: configure-local-kernel
configure-local-kernel: clean-local-kernel
	@mkdir -p $(local_kernel_b)
	@echo "Using config file $(LOCAL_KERNEL_CONFIG)"
	cp $(LOCAL_KERNEL_CONFIG) $(local_kernel_b)/.config
	$(kernel_d)/scripts/config \
		--file $(local_kernel_b)/.config \
		--disable CONFIG_DEBUG_INFO_BTF \
		--disable CONFIG_MODULE_SIG \
		--disable CONFIG_MODULE_SIG_ALL \
		--set-str CONFIG_SYSTEM_TRUSTED_KEYS "" \
		--set-str CONFIG_SYSTEM_REVOCATION_KEYS "" \
		--set-str CONFIG_MODULE_SIG_KEY ""
	$(kernel_d)/scripts/config \
		--file $(local_kernel_b)/.config \
		--disable CONFIG_LOCALVERSION_AUTO \
		--set-str CONFIG_LOCALVERSION "-ossim" \
		--enable CONFIG_OSSIM
	$(MAKE) -C $(kernel_d) O=$(abspath $(local_kernel_b)) olddefconfig

# Configure kernel with virtme-ng defaults (minimal config for fast builds)
.PHONY: configure-vng-kernel
configure-vng-kernel: clean-vng-kernel
	@mkdir -p $(vng_kernel_b)
	cd $(kernel_d) && $(VNG) --kconfig O=$(abspath $(vng_kernel_b))
	$(kernel_d)/scripts/config \
		--file $(vng_kernel_b)/.config \
		--disable CONFIG_LOCALVERSION_AUTO \
		--set-str CONFIG_LOCALVERSION "-ossim" \
		--enable CONFIG_OSSIM
	$(MAKE) -C $(kernel_d) O=$(abspath $(vng_kernel_b)) olddefconfig

$(vng_kernel_b)/.config:
	$(MAKE) configure-vng-kernel

$(local_kernel_b)/.config:
	$(MAKE) configure-local-kernel

# Build kernel
.PHONY: build-local-kernel
build-local-kernel: $(local_kernel_b)/.config
	$(MAKE) LD=ld.lld -C $(local_kernel_b) -j`nproc`
	$(MAKE) LD=ld.lld -C $(local_kernel_b) modules -j`nproc`

.PHONY: install-local-kernel
install-local-kernel: build-local-kernel
	$(SUDO) $(MAKE) -C $(local_kernel_b) modules_install install

.PHONY: build-vng-kernel
build-vng-kernel: $(vng_kernel_b)/.config
	$(MAKE) LD=ld.lld -C $(vng_kernel_b) -j`nproc`


.PHONY: clean-local-kernel
clean-local-kernel:
	rm -rf $(local_kernel_b)

.PHONY: clean-vng-kernel
clean-vng-kernel:
	rm -rf $(vng_kernel_b)

# Boot kernel with virtme-ng using host filesystem (read-only by default)
.PHONY: vng-kernel
vng-kernel:
ifeq ($(VNG_RW),1)
	$(VNG) --run $(abspath $(vng_kernel_b)) --rw $(VNG_OPTS)
else
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS)
endif

# Quick rebuild and test cycle
.PHONY: test-kernel
test-kernel: build-vng-kernel vng-kernel

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
vng-start:
	@mkdir -p $(vng_b)
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		echo "vng already running (pid=$$(cat $(VNG_PIDFILE)))"; \
		echo "Use 'make vng-ssh' to connect or 'make vng-stop' to stop"; \
	else \
		echo "Starting vng with SSH via vsock (cid=$(VNG_VSOCK_CID))..."; \
		nohup $(VNG) --run $(abspath $(vng_kernel_b)) --rw \
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
vng-exec:
	$(VNG) --run $(abspath $(vng_kernel_b)) --rw --exec "$(VNG_CMD)"

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
	@# Clean up any stale QEMU processes holding the vsock CID
	@STALE_PIDS=$$(pgrep -f "qemu.*guest-cid=$(VNG_VSOCK_CID)" 2>/dev/null); \
	if [ -n "$$STALE_PIDS" ]; then \
		echo "Cleaning up stale QEMU processes: $$STALE_PIDS"; \
		kill $$STALE_PIDS 2>/dev/null || true; \
		sleep 1; \
		kill -9 $$STALE_PIDS 2>/dev/null || true; \
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
