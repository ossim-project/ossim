$(o)%/secondary/disk.qcow2: $(ubuntu_base_secondary_img)
	mkdir -p $(@D)
	$(QEMU_IMG) create -f qcow2 -F qcow2 -b $(shell realpath --relative-to=$(dir $@) $<) $@

$(eval $(call include_rules,$(d)common/rules.mk))
$(eval $(call include_rules,$(d)controller/rules.mk))
$(eval $(call include_rules,$(d)compute/rules.mk))
