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
		--enable CONFIG_OSSIM \
		--enable CONFIG_OSSIM_DEBUG
	$(MAKE) -C $(kernel_d) O=$(abspath $(vng_kernel_b)) olddefconfig

$(vng_kernel_b)/.config:
	$(MAKE) configure-vng-kernel

$(local_kernel_b)/.config:
	$(MAKE) configure-local-kernel

# Build kernel
.PHONY: local-kernel
local-kernel: $(local_kernel_b)/.config
	$(MAKE) LD=ld.lld -C $(local_kernel_b) -j`nproc`
	$(MAKE) LD=ld.lld -C $(local_kernel_b) modules -j`nproc`

.PHONY: install-local-kernel
install-local-kernel:
	$(SUDO) $(MAKE) -C $(local_kernel_b) modules_install install

.PHONY: vng-kernel
vng-kernel: $(vng_kernel_b)/.config
	$(MAKE) stop-vng || true
	$(MAKE) LD=ld.lld -C $(vng_kernel_b) -j`nproc`


.PHONY: clean-local-kernel
clean-local-kernel:
	rm -rf $(local_kernel_b)

.PHONY: clean-vng-kernel
clean-vng-kernel:
	rm -rf $(vng_kernel_b)

# Boot kernel with virtme-ng using host filesystem (read-only by default)
.PHONY: run-vng
run-vng:
ifeq ($(VNG_RW),1)
	$(VNG) --run $(abspath $(vng_kernel_b)) --rw $(VNG_OPTS)
else
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS)
endif

# Quick rebuild and test cycle
.PHONY: test-kernel
test-kernel: vng-kernel run-vng

# Persistent vng instance with SSH access via TCP port
VNG_SSH_PORT ?= 12222
VNG_MEM ?= 8G
VNG_CPUS ?= 8
vng_b := $(b)vng/
VNG_PIDFILE := $(vng_b)vng.pid
VNG_LOG := $(vng_b)vng.log

# Start vng as a background instance with SSH access via TCP
.PHONY: start-vng
start-vng:
	@mkdir -p $(vng_b)
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		echo "vng already running (pid=$$(cat $(VNG_PIDFILE)))"; \
		echo "Use 'make ssh-vng' to connect or 'make stop-vng' to stop"; \
	else \
		echo "Starting vng with SSH via TCP (port=$(VNG_SSH_PORT))..."; \
		nohup $(VNG) --run $(abspath $(vng_kernel_b)) --rw \
			--memory $(VNG_MEM) --cpus $(VNG_CPUS) \
			--ssh $(VNG_SSH_PORT) --ssh-tcp $(VNG_OPTS) \
			> $(VNG_LOG) 2>&1 & \
		echo $$! > $(VNG_PIDFILE); \
		sleep 5; \
		if kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
			echo "vng started (pid=$$(cat $(VNG_PIDFILE))). Configuring /dev/kvm..."; \
			ssh -p $(VNG_SSH_PORT) \
				-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
				-o ConnectTimeout=10 \
				$(USER)@localhost "sudo chmod 666 /dev/kvm" 2>/dev/null || true; \
			echo "Ready. Use 'make ssh-vng' to connect."; \
		else \
			echo "vng failed to start. Check $(VNG_LOG)"; \
			rm -f $(VNG_PIDFILE); \
		fi \
	fi

# SSH into the running vng instance via TCP with PTY and environment variables
# Starts in the current working directory
.PHONY: ssh-vng
ssh-vng:
	@if [ -f $(VNG_PIDFILE) ] && kill -0 $$(cat $(VNG_PIDFILE)) 2>/dev/null; then \
		ssh -t -p $(VNG_SSH_PORT) \
			-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
			$(USER)@localhost "cd $(CURDIR) && exec bash -l"; \
	else \
		echo "No vng instance running. Use 'make start-vng' first."; \
	fi

# Run a command in vng (uses fresh instance, no persistent connection needed)
# Usage: make vng-exec VNG_CMD="uname -a"
.PHONY: exec-vng
exec-vng:
	$(VNG) --run $(abspath $(vng_kernel_b)) --rw --exec "$(VNG_CMD)"

# Stop the running vng instance
.PHONY: stop-vng
stop-vng:
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
	@# Kill any SSH connections to the vng port
	@STALE_SSH=$$(pgrep -f "ssh.*$(VNG_SSH_PORT)" 2>/dev/null); \
	if [ -n "$$STALE_SSH" ]; then \
		echo "Cleaning up stale SSH connections: $$STALE_SSH"; \
		kill $$STALE_SSH 2>/dev/null || true; \
	fi
	@# Clean up any stale QEMU processes using the SSH port (match various hostfwd formats)
	@STALE_PIDS=$$(pgrep -f "qemu.*:$(VNG_SSH_PORT)" 2>/dev/null); \
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
		echo "vng running (pid=$$(cat $(VNG_PIDFILE)), ssh port=$(VNG_SSH_PORT))"; \
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

.PHONY: watch-dmesg
watch-dmesg:
	watch -n 1 "sudo dmesg | tail -20"
	