# OSSIM make-system variables:
#
# Required local build variables:
#   OSSIM_PREFIX                 Install prefix used by local build rules.
#   OSSIM_BUILD_DIR              Build directory root.
#   OSSIM_OUT_DIR                Output/install-staging directory root.
#
# Target dispatch variables:
#   OSSIM_TARGET_LOGIN           SSH login for the target host, e.g. user@host.
#   OSSIM_TARGET_DIR             Repository path on the target host.
#   OSSIM_TARGET_SYNC            1 = rsync this workspace before remote target; 0 = skip rsync.
#   OSSIM_TARGET_RSYNC_ARGS      Args for rsync when OSSIM_TARGET_SYNC=1.
#   OSSIM_TARGET_SSH_ARGS        SSH args for target execution.
#   OSSIM_TARGET_TTY_TARGETS     Target-side make goals that should allocate ssh -t.
#
# KGDB variables:
#   OSSIM_TARGET_KGDB_PORT       Target kernel serial port for kgdboc, e.g. ttyS5.
#   OSSIM_DEV_LOGIN              SSH login for the dev/debug host, e.g. user@host.
#   OSSIM_DEV_DIR                Repository path on the dev host.
#   OSSIM_TARGET_DIR             Repository path on the target host.
#   OSSIM_DEV_KGDB_PORT          Dev-side KGDB serial device, e.g. ttyUSB0.
#   OSSIM_DEV_KGDB_VMLINUX       Dev-side unstripped vmlinux path for GDB.
#   OSSIM_KGDB_BAUD              KGDB serial baudrate; must match target kgdboc baud.
#
# Target dispatch: `make target-<goal>` runs <goal> on $(OSSIM_TARGET_LOGIN)
# over SSH. Set OSSIM_TARGET_SYNC=1 to rsync this workspace to
# $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR) before running the target. When
# OSSIM_TARGET_SYNC=0, the target directory is assumed to already exist and be
# up to date, e.g. via a shared filesystem.

OSSIM_TARGET_MAKE_ARGS ?=
OSSIM_TARGET_RSYNC_PUSH_ARGS ?= -azv --delete --filter=':- .gitignore'
OSSIM_TARGET_SSH_ARGS ?=
OSSIM_TARGET_TTY_TARGETS ?= run-vng run-vng-gdb run-vng-gdb-paused gdb-vng kgdb-kernel kgdb-remote-kernel kexec-local-kernel-kgdb test-kernel ssh-vng
OSSIM_TARGET_FORWARD_ARGS := $(filter-out OSSIM_TARGET_%,$(MAKEOVERRIDES))

OSSIM_TARGET_RSYNC_PULL_ARGS ?= -azv --delete --exclude='.git/' --filter=':- .gitignore'

ifneq ($(filter target-%,$(MAKECMDGOALS)),)

check-target-vars:
	@if [ -z "$(OSSIM_TARGET_LOGIN)" ]; then \
		echo "Error: OSSIM_TARGET_LOGIN is not set (e.g. OSSIM_TARGET_LOGIN=user@host)"; \
		exit 1; \
	fi
	@if [ -z "$(OSSIM_TARGET_DIR)" ]; then \
		echo "Error: OSSIM_TARGET_DIR is not set"; \
		exit 1; \
	fi
	@if [ -z "$(OSSIM_TARGET_SYNC)" ]; then \
		echo "Error: OSSIM_TARGET_SYNC is not set (0 = skip rsync, 1 = rsync target dir)"; \
		exit 1; \
	fi
.PHONY: check-target-vars

target-push: check-target-vars
	@if [ "$(OSSIM_TARGET_SYNC)" = "1" ]; then \
		echo "Syncing local workspace to $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)..."; \
		ssh $(OSSIM_TARGET_LOGIN) 'mkdir -p $(OSSIM_TARGET_DIR)'; \
		rsync $(OSSIM_TARGET_RSYNC_PUSH_ARGS) ./ $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)/; \
	else \
		echo "Skipping target sync (OSSIM_TARGET_SYNC=$(OSSIM_TARGET_SYNC)); using $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)"; \
	fi
.PHONY: target-push

target-pull: check-target-vars
	@if [ "$(OSSIM_TARGET_SYNC)" = "1" ]; then \
		echo "Syncing local workspace from $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)..."; \
		rsync $(OSSIM_TARGET_RSYNC_PULL_ARGS) $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)/ ./; \
	else \
		echo "Skipping target sync pull (OSSIM_TARGET_SYNC=$(OSSIM_TARGET_SYNC)); using $(OSSIM_TARGET_LOGIN):$(OSSIM_TARGET_DIR)"; \
	fi
.PHONY: target-pull

target-%: target-push
	@echo "Running 'make $*' on $(OSSIM_TARGET_LOGIN) over SSH..."; \
	ssh_args="$(OSSIM_TARGET_SSH_ARGS)"; \
	if [ -z "$$ssh_args" ]; then \
		case " $(OSSIM_TARGET_TTY_TARGETS) " in \
			*" $* "*) ssh_args="-t" ;; \
		esac; \
	fi; \
	ssh $$ssh_args $(OSSIM_TARGET_LOGIN) 'cd $(OSSIM_TARGET_DIR) && make $* $(OSSIM_TARGET_FORWARD_ARGS)'
.PHONY: target-%

else  # local build — require ossim env vars and load build rules

ifeq ($(OSSIM_PREFIX),)
$(error OSSIM_PREFIX is not set)
endif
ifeq ($(OSSIM_BUILD_DIR),)
$(error OSSIM_BUILD_DIR is not set)
endif
ifeq ($(OSSIM_OUT_DIR),)
$(error OSSIM_OUT_DIR is not set)
endif

PREFIX := $(abspath $(OSSIM_PREFIX))
BUILD := $(OSSIM_BUILD_DIR)
OUTPUT := $(OSSIM_OUT_DIR)

include make/include.mk

all:
	@echo Hello Ossim
.PHONY: all

CLANG_FORMAT ?= clang-format
CLANG_FORMAT_STYLE ?= $(project_root).clang-format

SUDO_ENV ?= LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PATH=$(PATH) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
ifdef SUDO_PASS
SUDO ?= sh -c 'echo "$(SUDO_PASS)" | /usr/bin/sudo.ws -S $(SUDO_ENV) "$$@"' _
else
SUDO ?= sudo $(SUDO_ENV)
endif

clean:
	rm -rf $(CLEAN_ALL)
.PHONY: clean

include make/qemu.mk
include make/kernel.mk
include make/libossim.mk

endif
