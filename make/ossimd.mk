ossimd_d := $(d)ossimd/
ossimd_b := $(b)ossimd/

.PHONY: ossimd
ossimd:
	$(MAKE) O=$(ossimd_b) -C $(ossimd_d) all

.PHONY: clean-ossimd
clean-ossimd:
	$(MAKE) O=$(ossimd_b) -C $(ossimd_d) clean

.PHONY: run-ossimd
run-ossimd:
	@$(SUDO) $(MAKE) O=$(ossimd_b) -C $(ossimd_d) run

.PHONY: format-ossimd
format-ossimd:
	$(CLANG_FORMAT) -i --style=file $(shell find $(ossimd_d) -name '*.c' -or -name '*.h')
