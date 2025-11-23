libossim_d := $(d)lib/ossim/

# Build libossim (required for ossim integration)
.PHONY: build-libossim
build-libossim:
	$(MAKE) -C $(libossim_d)

# Install libossim to PREFIX/lib
.PHONY: install-libossim
install-libossim: build-libossim
	@mkdir -p $(PREFIX)lib
	cp $(libossim_d)libossim.so $(PREFIX)lib/
	cp $(libossim_d)libossim.a $(PREFIX)lib/

# Clean libossim
.PHONY: clean-libossim
clean-libossim:
	$(MAKE) -C $(libossim_d) clean
