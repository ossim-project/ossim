# libossim - Ossim control library and daemon
#
# This wraps the CMake-based libossim build system

libossim_d := $(d)libossim
libossim_b := $(b)libossim

OSSIM_YIELD_INTERVAL ?= 10000
OSSIM_YIELD_DURATION ?= 1

# Kernel UAPI headers for ossimd
LIBOSSIM_KERNEL_UAPI := $(abspath $(kernel_d)/include/uapi)

# CMake configuration
LIBOSSIM_CMAKE_ARGS := \
	-DCMAKE_BUILD_TYPE=Release \
	-DKERNEL_UAPI_INCLUDE_DIR=$(LIBOSSIM_KERNEL_UAPI) \
	-DCMAKE_INSTALL_PREFIX=$(PREFIX)

# Configure libossim
.PHONY: configure-libossim
configure-libossim: clean-libossim
	@mkdir -p $(libossim_b)
	cd $(libossim_b) && cmake $(LIBOSSIM_CMAKE_ARGS) $(abspath $(libossim_d))

# Build libossim (configure if needed)
.PHONY: libossim
libossim:
	@if [ ! -f $(libossim_b)/Makefile ]; then \
		$(MAKE) configure-libossim; \
	fi
	$(MAKE) -C $(libossim_b) -j$$(nproc)

# Build individual components
.PHONY: ossimd
ossimd: libossim

.PHONY: ossimctl
ossimctl: libossim

# Install libossim to PREFIX
.PHONY: install-libossim
install-libossim: libossim
	$(MAKE) -C $(libossim_b) install

# Run ossimd
.PHONY: run-ossimd
run-ossimd: libossim
	$(SUDO) $(libossim_b)/src/daemon/ossimd

# Clean libossim
.PHONY: clean-libossim
clean-libossim:
	rm -rf $(libossim_b)

# Full clean (remove build directory)
.PHONY: distclean-libossim
distclean-libossim:
	rm -rf $(libossim_b)
