libossim_d := $(d)lib/ossim/
libossim_b := $(b)lib/ossim/

# Build libossim (required for ossim integration)
.PHONY: lib
lib:
	$(MAKE) -C $(libossim_d) O=$(libossim_b)

# Install libossim to PREFIX
.PHONY: install-lib
install-lib: lib
	$(MAKE) -C $(libossim_d) O=$(libossim_b) PREFIX=$(PREFIX) install

.PHONY: lib-test
lib-test: lib
	$(MAKE) -C $(libossim_d) O=$(libossim_b) test-build

.PHONY: clean-lib-test
clean-lib-test:
	$(MAKE) -C $(libossim_d) O=$(libossim_b) test-clean

# Clean libossim
.PHONY: clean-lib
clean-lib: clean-lib-test
	$(MAKE) -C $(libossim_d) O=$(libossim_b) clean
