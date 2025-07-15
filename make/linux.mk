LINUX_TARGET := vmlinux

linux_d := $(d)linux/
linux_b := $(b)linux/
linux_o := $(o)linux/

linux_vmlinux := $(linux_o)vmlinux
$(linux_vmlinux): $(linux_b)vmlinux
	@mkdir -p $(@D)
	cp $< $@

$(linux_b).config:
	$(MAKE) configure-linux

$(linux_b)vmlinux:
	$(MAKE) build-linux

.PHONY: build-linux
build-linux: $(linux_b).config
	$(MAKE) LD=ld.lld -C $(linux_b) -j`nproc` $(LINUX_TARGET)

.PHONY: clean-linux
clean-linux:
	rm -rf $(linux_b) $(linux_o)
