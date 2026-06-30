# Remote dispatch: `make remote-<target>` runs <target> on $(OSSIM_REMOTE_TARGET) over SSH.
# Intended for local-edit / remote-build workflows where this workspace is shared
# with the build host (e.g. via NFS). $(OSSIM_REMOTE_DIR) defaults to $(CURDIR),
# which is correct when the workspace is mounted at the same path on both sides.

OSSIM_REMOTE_TARGET ?=
OSSIM_REMOTE_DIR ?= $(CURDIR)
OSSIM_REMOTE_MAKE_ARGS ?=
OSSIM_REMOTE_RSYNC_ARGS ?= -azv --delete --filter=':- .gitignore'

ifneq ($(filter remote-%,$(MAKECMDGOALS)),)

check-remote-vars:
	@if [ -z "$(OSSIM_REMOTE_TARGET)" ]; then \
		echo "Error: OSSIM_REMOTE_TARGET is not set (e.g. via OSSIM_REMOTE_TARGET=lab.host)"; \
		exit 1; \
	fi
	@if [ -z "$(OSSIM_REMOTE_DIR)" ]; then \
		echo "Error: OSSIM_REMOTE_DIR is not set"; \
		exit 1; \
	fi
.PHONY: check-remote-vars

remote-sync: check-remote-vars
	@echo "Syncing local workspace to $(OSSIM_REMOTE_TARGET):$(OSSIM_REMOTE_DIR)..."
	ssh $(OSSIM_REMOTE_TARGET) 'mkdir -p $(OSSIM_REMOTE_DIR)'
	rsync $(OSSIM_REMOTE_RSYNC_ARGS) ./ $(OSSIM_REMOTE_TARGET):$(OSSIM_REMOTE_DIR)/
.PHONY: remote-sync

remote-%: remote-sync
	@echo "Running 'make $*' on $(OSSIM_REMOTE_TARGET) over SSH..."; \
	ssh -t $(OSSIM_REMOTE_TARGET) 'cd $(OSSIM_REMOTE_DIR) && make $* $(OSSIM_REMOTE_MAKE_ARGS)'
.PHONY: remote-%

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
