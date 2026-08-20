# ns-3 - the modeled network component (dummy-node mode)
#
# This wraps the CMake-based ns-3 build system. The build is fully
# out-of-tree: the cmake cache lives in $(BUILD)ns-3 and the built
# artifacts in $(BUILD)ns-3/out, so the source tree stays clean and
# the build lands on local disk like the kernel/qemu/libossim builds.
# Scenario apps (simulation content) live in workloads/ns3 and are
# built by the WORKLOADS make system against the installed ns-3:
# `make -C workloads ns3` (never from here -- the superproject does
# not drive workloads builds).

ns3_d := $(d)ns-3
ns3_b := $(b)ns-3

# The module set the ossim legs need; semicolon-separated, override to
# widen. The contrib module "ossim" is the adapter itself.
NS3_MODULES ?= core;network;internet;point-to-point;csma;bridge;applications;internet-apps;traffic-control;flow-monitor;ossim

# release matches the ./ns3 wrapper's release profile (asserts/logging
# off). Set NS3_NATIVE=ON for -march=native (the wrapper's "optimized"
# profile) when the build host is the run host.
NS3_NATIVE ?= OFF

NS3_CMAKE_ARGS := \
	-DCMAKE_BUILD_TYPE=release \
	-DNS3_ASSERT=OFF \
	-DNS3_LOG=OFF \
	-DNS3_WARNINGS_AS_ERRORS=OFF \
	-DNS3_NATIVE_OPTIMIZATIONS=$(NS3_NATIVE) \
	-DNS3_ENABLED_MODULES="$(NS3_MODULES)" \
	-DNS3_EXAMPLES=OFF \
	-DNS3_TESTS=OFF \
	-DNS3_OUTPUT_DIRECTORY=$(abspath $(ns3_b))/out \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(PREFIX))

# Configure ns-3
.PHONY: configure-ns3
configure-ns3: clean-ns3
	@mkdir -p $(ns3_b)
	cmake -S $(abspath $(ns3_d)) -B $(ns3_b) $(NS3_CMAKE_ARGS)

# Build ns-3 (configure if needed); shared libs land under
# $(BUILD)ns-3/out/lib, headers/libs install to PREFIX via
# install-ns3.
.PHONY: ns3
ns3:
	@if [ ! -f $(ns3_b)/CMakeCache.txt ]; then \
		$(MAKE) configure-ns3; \
	fi
	cmake --build $(ns3_b) -j $(JOBS)

# Install ns-3 libraries and headers to PREFIX
.PHONY: install-ns3
install-ns3: ns3
	cmake --install $(ns3_b)

# Clean ns-3
.PHONY: clean-ns3
clean-ns3:
	rm -rf $(ns3_b)

.PHONY: distclean-ns3
distclean-ns3:
	rm -rf $(ns3_b)
