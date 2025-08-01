kmod_d := $(d)kmod/
kmod_b := $(b)kmod/

.PHONY: build-kmod
build-kmod:
	@mkdir -p $(kmod_b)
	$(MAKE) -C $(kmod_d) MO=$(abspath $(kmod_b)) -j`nproc`

.PHONY: clean-kmod
clean-kmod:
	$(MAKE) -C $(kmod_d) MO=$(abspath $(kmod_b)) clean

.PHONY: load-kmod
load-kmod: $(kmod_b)ossim.ko
	sudo insmod $<
	sudo dmesg | tail -n 10

.PHONY: unload-kmod
unload-kmod:
	sudo rmmod ossim
	sudo dmesg | tail -n 10

.PHONY: reload-kmod
reload-kmod: unload-kmod load-kmod

$(kmod_b)ossim.ko:
	$(MAKE) build-kmod