# kernel.mk - Build and test custom kernel with virtme-ng
HOST_KERNEL_CONFIG := /boot/config-$(shell uname -r)
BOOT_DIR := /boot

kernel_d := $(d)kernel

DEBUG ?= 0

local_kernel_normal_b := $(b)kernel.local
local_kernel_debug_b := $(b)kernel.local.debug
vng_kernel_normal_b := $(b)kernel.vng
vng_kernel_debug_b := $(b)kernel.vng.debug

ifeq ($(DEBUG),1)
local_kernel_b := $(local_kernel_debug_b)
vng_kernel_b := $(vng_kernel_debug_b)
KERNEL_LOCALVERSION := -ossim.debug
else
local_kernel_b := $(local_kernel_normal_b)
vng_kernel_b := $(vng_kernel_normal_b)
KERNEL_LOCALVERSION := -ossim
endif

# virtme-ng configuration
VNG_MEM ?= 8G
VNG_CPUS ?= 8
VNG ?= vng
VNG_CONFIGKERNEL ?= virtme-configkernel

VNG_RW ?= 1
VNG_KERNEL_CMDLINE_APPEND += ossim_cpus=4-7
VNG_KERNEL_CMDLINE_APPEND_OPT = $(if $(strip $(VNG_KERNEL_CMDLINE_APPEND)),--append "$(VNG_KERNEL_CMDLINE_APPEND)")

VNG_OPTS ?= $(if $(filter 1,$(VNG_RW)),--rw) --memory $(VNG_MEM) --cpus $(VNG_CPUS) \
	$(VNG_KERNEL_CMDLINE_APPEND_OPT)

GDB ?= gdb
VNG_GDB_OPTS ?= --append nokaslr --qemu-opts='-s'
VNG_GDB_PAUSED_OPTS ?= --append nokaslr --qemu-opts='-s -S'
VNG_GDB_HOST ?= localhost
VNG_GDB_PORT ?= 1234
OSSIM_KGDB_BAUD ?= 115200

# tracefs utilities for OSSIM/KVM timer and scheduler diagnostics.
# Override OSSIM_TRACEFS for systems that mount tracefs elsewhere, and override
# OSSIM_TRACEPOINTS with a whitespace-separated list of subsystem:event names.
OSSIM_TRACEFS ?= /sys/kernel/tracing
OSSIM_TRACEPOINTS ?= \
	ossim:ossim_idle_jump \
	ossim:ossim_vtime_forward \
	ossim:ossim_lapic_timer_cancel \
	ossim:ossim_lapic_timer_reset \
	ossim:ossim_lapic_timer_due \
	ossim:ossim_lapic_timer_expire \
	ossim:ossim_sched_vtime_refresh \
	ossim:ossim_sched_rq

# KVM_OSSIM (the KVM vCPU integration) depends on VIRT_CPU_ACCOUNTING_GEN and
# PARAVIRT_TIME_ACCOUNTING.  GEN is a choice member, so Kconfig will not
# auto-select it; every config path must explicitly pick GEN and disable the
# competing TICK choice.  PARAVIRT_TIME_ACCOUNTING makes get_vtime_delta()
# subtract L0 steal from guest cputime, so vtime stays guest-execution-only
# when the simulation host itself runs on a hypervisor (nested virt). Inert on
# bare metal.
KERNEL_CONFIG_OPTS := \
	--disable CONFIG_LOCALVERSION_AUTO \
	--set-str CONFIG_LOCALVERSION "$(KERNEL_LOCALVERSION)" \
	--disable CONFIG_TICK_CPU_ACCOUNTING \
	--enable CONFIG_VIRT_CPU_ACCOUNTING_GEN \
	--enable CONFIG_PARAVIRT_TIME_ACCOUNTING \
	--enable CONFIG_OSSIM \
	--enable CONFIG_KVM \
	--enable CONFIG_KVM_INTEL \
	--enable CONFIG_KVM_AMD \
	--enable CONFIG_KVM_OSSIM

KERNEL_DEBUG_CONFIG_OPTS := \
	--enable CONFIG_OSSIM_DEBUG \
	--enable CONFIG_KUNIT \
	--enable CONFIG_OSSIM_KUNIT_TEST \
	--enable CONFIG_DEBUG_KERNEL \
	--enable CONFIG_DEBUG_INFO \
	--disable CONFIG_DEBUG_INFO_NONE \
	--disable CONFIG_DEBUG_INFO_REDUCED \
	--disable CONFIG_DEBUG_INFO_SPLIT \
	--enable CONFIG_DEBUG_INFO_DWARF5 \
	--enable CONFIG_DEBUG_INFO_COMPRESSED_NONE \
	--enable CONFIG_GDB_SCRIPTS \
	--enable CONFIG_FRAME_POINTER \
	--enable CONFIG_KALLSYMS \
	--enable CONFIG_KALLSYMS_ALL \
	--disable CONFIG_RANDOMIZE_BASE

# Configure kernel from host config with disabled problematic options
.PHONY: configure-local-kernel
configure-local-kernel: clean-local-kernel
	@mkdir -p $(local_kernel_b)
	@echo "Using config file $(HOST_KERNEL_CONFIG)"
	cp $(HOST_KERNEL_CONFIG) $(local_kernel_b)/.config
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
		$(KERNEL_CONFIG_OPTS)
ifeq ($(DEBUG),1)
	@echo "Enabling local kernel debug info for gdb (DEBUG=1)"
	$(kernel_d)/scripts/config \
		--file $(local_kernel_b)/.config \
		$(KERNEL_DEBUG_CONFIG_OPTS) \
		--enable CONFIG_KGDB \
		--enable CONFIG_KGDB_SERIAL_CONSOLE \
		--disable CONFIG_STRICT_KERNEL_RWX
endif
	$(MAKE) -C $(kernel_d) O=$(abspath $(local_kernel_b)) olddefconfig

# Configure kernel with virtme-ng defaults (minimal config for fast builds)
.PHONY: configure-vng-kernel
configure-vng-kernel: clean-vng-kernel
	@mkdir -p $(vng_kernel_b)
	cd $(kernel_d) && KBUILD_OUTPUT=$(abspath $(vng_kernel_b)) \
		$(VNG_CONFIGKERNEL) --defconfig O=$(abspath $(vng_kernel_b))
	$(kernel_d)/scripts/config \
		--file $(vng_kernel_b)/.config \
		$(KERNEL_CONFIG_OPTS)
ifeq ($(DEBUG),1)
	@echo "Enabling VNG kernel debug info for gdb (DEBUG=1)"
	$(kernel_d)/scripts/config \
		--file $(vng_kernel_b)/.config \
		$(KERNEL_DEBUG_CONFIG_OPTS)
endif
	$(MAKE) -C $(kernel_d) O=$(abspath $(vng_kernel_b)) olddefconfig

$(vng_kernel_b)/.config:
	$(MAKE) configure-vng-kernel

$(local_kernel_b)/.config:
	$(MAKE) configure-local-kernel

# Build kernel
.PHONY: local-kernel
local-kernel: $(local_kernel_b)/.config
	$(MAKE) LD=ld.lld -C $(local_kernel_b) -j$(JOBS) bzImage

.PHONY: local-kernel-all
local-kernel-all: $(local_kernel_b)/.config
	$(MAKE) LD=ld.lld -C $(local_kernel_b) -j$(JOBS)
	$(MAKE) LD=ld.lld -C $(local_kernel_b) modules -j$(JOBS)

.PHONY: install-local-kernel
install-local-kernel: local-kernel
	KERNEL_RELEASE=$$($(MAKE) -s -C $(local_kernel_b) kernelrelease) && \
		sudo install -m 0644 $(local_kernel_b)/arch/x86/boot/bzImage "$(BOOT_DIR)/vmlinuz-$$KERNEL_RELEASE"

.PHONY: install-local-kernel-all
install-local-kernel-all: local-kernel-all
	$(SUDO) $(MAKE) -C $(local_kernel_b) modules_install install


# Boot the installed local kernel via kexec
.PHONY: kexec-local-kernel
kexec-local-kernel:
	@KERNEL_RELEASE=$$($(MAKE) -s -C $(local_kernel_b) kernelrelease) && \
		$(MAKE) KERNEL_RELEASE="$$KERNEL_RELEASE" \
			KERNEL_CMDLINE="$(OSSIM_KEXEC_KERNEL_CMDLINE)" \
			kexec-kernel

.PHONY: kexec-default-kernel
kexec-default-kernel:
	$(MAKE) KERNEL_RELEASE="$(OSSIM_KEXEC_DEFAULT_KERNEL)" \
		KERNEL_CMDLINE="$(OSSIM_KEXEC_DEFAULT_KERNEL_CMDLINE)" \
		kexec-kernel

.PHONY: kexec-kernel
kexec-kernel:
	@if [ -z "$(KERNEL_RELEASE)" ]; then \
		echo "Error: KERNEL_RELEASE is not set. Aborted."; \
		exit 1; \
	fi; \
	if [ -n "$(KERNEL_CMDLINE)" ]; then \
		set -- --append "$(KERNEL_CMDLINE)"; \
	else \
		set -- --reuse-cmdline; \
	fi; \
	echo "Loading kernel $(KERNEL_RELEASE) via kexec..." && \
	$(SUDO) kexec -l /boot/vmlinuz-$(KERNEL_RELEASE) \
		--initrd=/boot/initrd.img-$(KERNEL_RELEASE) \
		"$$@" && \
	echo "Switching to $(KERNEL_RELEASE) via kexec..." && \
	$(SUDO) systemctl kexec

.PHONY: kexec-local-kernel-kgdb
kexec-local-kernel-kgdb: rsync-local-vmlinux
ifeq ($(strip $(OSSIM_TARGET_KGDB_PORT)),)
	$(error OSSIM_TARGET_KGDB_PORT is not defined! Pass it like: make kexec-local-kernel-kgdb OSSIM_TARGET_KGDB_PORT=ttyS5)
endif
	@KERNEL_RELEASE=$$($(MAKE) -s -C $(local_kernel_b) kernelrelease) && \
		$(MAKE) KERNEL_RELEASE="$$KERNEL_RELEASE" \
			KERNEL_CMDLINE="$(OSSIM_KEXEC_KERNEL_CMDLINE) console=$(OSSIM_TARGET_KGDB_PORT),$(OSSIM_KGDB_BAUD) kgdboc=$(OSSIM_TARGET_KGDB_PORT),$(OSSIM_KGDB_BAUD) kgdbwait" \
			kexec-kernel

.PHONY: kexec-local-kernel-gdb
kexec-local-kernel-gdb: kexec-local-kernel-kgdb

# Attach gdb to a kernel KGDB endpoint. For serial KGDB, OSSIM_KGDB_BAUD
# must match the kernel kgdboc baud, e.g. kgdboc=ttyS4,115200.
.PHONY: kgdb-kernel
kgdb-kernel:
ifeq ($(strip $(OSSIM_DEV_KGDB_VMLINUX)),)
	$(error OSSIM_DEV_KGDB_VMLINUX is not defined! Pass it like: make kgdb-kernel OSSIM_DEV_KGDB_VMLINUX=/path/to/vmlinux)
endif
ifeq ($(strip $(OSSIM_DEV_KGDB_PORT)),)
	$(error OSSIM_DEV_KGDB_PORT is not defined! Pass it like: make kgdb-kernel OSSIM_DEV_KGDB_PORT=ttyUSB0)
endif
	@DEV_PORT="$(OSSIM_DEV_KGDB_PORT)"; \
	case "$$DEV_PORT" in /*|tcp:*) ;; *) DEV_PORT="/dev/$$DEV_PORT" ;; esac; \
	$(GDB) "$(OSSIM_DEV_KGDB_VMLINUX)" \
		-ex "set serial baud $(OSSIM_KGDB_BAUD)" \
		-ex "target remote $$DEV_PORT"

.PHONY: rsync-local-vmlinux
rsync-local-vmlinux:
ifeq ($(strip $(OSSIM_DEV_LOGIN)),)
	$(error OSSIM_DEV_LOGIN is not defined! Pass it like: make rsync-local-vmlinux OSSIM_DEV_LOGIN=user@host)
endif
ifeq ($(strip $(OSSIM_DEV_KGDB_VMLINUX)),)
	$(error OSSIM_DEV_KGDB_VMLINUX is not defined! Pass it like: make rsync-local-vmlinux OSSIM_DEV_KGDB_VMLINUX=/path/to/vmlinux)
endif
	@KERNEL_RELEASE=$$($(MAKE) -s -C $(local_kernel_b) kernelrelease) && \
		VMLINUX_SRC="$(local_kernel_b)/vmlinux" && \
		if [ ! -f "$$VMLINUX_SRC" ]; then \
			echo "Error: $$VMLINUX_SRC not found! Please build the kernel first." >&2; \
			exit 1; \
		fi; \
		echo "Syncing unstripped vmlinux ($$KERNEL_RELEASE) to $(OSSIM_DEV_LOGIN):$(OSSIM_DEV_KGDB_VMLINUX)..." && \
		rsync -avz --progress "$$VMLINUX_SRC" "$(OSSIM_DEV_LOGIN):$(OSSIM_DEV_KGDB_VMLINUX)"

.PHONY: vng-kernel
vng-kernel: $(vng_kernel_b)/.config
	$(MAKE) stop-vng || true
	$(MAKE) LD=ld.lld -C $(vng_kernel_b) -j$(JOBS)


.PHONY: clean-local-kernel
clean-local-kernel:
	rm -rf $(local_kernel_b)

.PHONY: clean-vng-kernel
clean-vng-kernel:
	rm -rf $(vng_kernel_b)

# Boot kernel with virtme-ng using host filesystem (read-only by default)
.PHONY: run-vng
run-vng:
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS)

# Boot kernel under virtme-ng with QEMU's gdbstub enabled.
.PHONY: run-vng-gdb
run-vng-gdb:
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS) $(VNG_GDB_OPTS)

# Boot kernel under virtme-ng with QEMU's gdbstub and pause at reset.
.PHONY: run-vng-gdb-paused
run-vng-gdb-paused:
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS) $(VNG_GDB_PAUSED_OPTS)

# Attach gdb to a virtme-ng/QEMU gdbstub started by run-vng-gdb.
.PHONY: gdb-vng
gdb-vng:
	$(GDB) $(abspath $(vng_kernel_b))/vmlinux \
		-ex "target remote $(VNG_GDB_HOST):$(VNG_GDB_PORT); continue"

# Quick rebuild and test cycle
.PHONY: test-kernel
test-kernel: vng-kernel run-vng

# Persistent vng instance with SSH access via TCP port
VNG_SSH_PORT ?= 12222
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
		nohup $(VNG) --run $(abspath $(vng_kernel_b)) \
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
	$(VNG) --run $(abspath $(vng_kernel_b)) $(VNG_OPTS) --exec "$(VNG_CMD)"

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

# Clear the trace buffer, enable OSSIM_TRACEPOINTS, and start tracing. Validate
# every event before changing tracefs so a typo cannot leave a partial setup.
.PHONY: start-kernel-trace
start-kernel-trace:
	@TRACEFS="$(OSSIM_TRACEFS)"; \
	if [ ! -d "$$TRACEFS" ]; then \
		echo "Error: tracefs not found at $$TRACEFS. Set OSSIM_TRACEFS to its mount point." >&2; \
		exit 1; \
	fi; \
	if [ -z "$(strip $(OSSIM_TRACEPOINTS))" ]; then \
		echo "Error: OSSIM_TRACEPOINTS is empty." >&2; \
		exit 1; \
	fi; \
	for event in $(OSSIM_TRACEPOINTS); do \
		if ! $(SUDO) grep -qx "$$event" "$$TRACEFS/available_events"; then \
			echo "Error: tracepoint '$$event' is not available." >&2; \
			exit 1; \
		fi; \
	done; \
	printf '0\n' | $(SUDO) tee "$$TRACEFS/tracing_on" >/dev/null; \
	printf '\n' | $(SUDO) tee "$$TRACEFS/set_event" >/dev/null; \
	printf '\n' | $(SUDO) tee "$$TRACEFS/trace" >/dev/null; \
	for event in $(OSSIM_TRACEPOINTS); do \
		printf '%s\n' "$$event" | $(SUDO) tee -a "$$TRACEFS/set_event" >/dev/null; \
	done; \
	printf '1\n' | $(SUDO) tee "$$TRACEFS/tracing_on" >/dev/null; \
	echo "Tracing enabled at $$TRACEFS:"; \
	for event in $(OSSIM_TRACEPOINTS); do echo "  $$event"; done

# Stop tracing and disable all tracepoint events. The captured trace remains in
# the buffer and can be inspected with `make view-trace`.
.PHONY: stop-kernel-trace
stop-kernel-trace:
	@TRACEFS="$(OSSIM_TRACEFS)"; \
	if [ ! -d "$$TRACEFS" ]; then \
		echo "Error: tracefs not found at $$TRACEFS. Set OSSIM_TRACEFS to its mount point." >&2; \
		exit 1; \
	fi; \
	printf '0\n' | $(SUDO) tee "$$TRACEFS/tracing_on" >/dev/null; \
	printf '\n' | $(SUDO) tee "$$TRACEFS/set_event" >/dev/null; \
	echo "Tracing disabled; captured data remains in $$TRACEFS/trace."

# Print the current trace snapshot. Disable tracing first for a stable capture.
.PHONY: print-kernel-trace
print-kernel-trace:
	@TRACEFS="$(OSSIM_TRACEFS)"; \
	if [ ! -e "$$TRACEFS/trace" ]; then \
		echo "Error: trace not found at $$TRACEFS/trace. Set OSSIM_TRACEFS to its mount point." >&2; \
		exit 1; \
	fi; \
	$(SUDO) cat "$$TRACEFS/trace"
