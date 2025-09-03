#>>> Templates

define bigdata_node_rule
$(eval node := $(1))
$(eval install_disk := $(2))
$(eval management_mac := $(call conffget,host,.qemu_mac_list[$(3)]))

.PRECIOUS: $(bigdata_dimg_o)setup/$(node)/disk.qcow2
$(bigdata_dimg_o)setup/$(node)/disk.qcow2: $(bigdata_dimg_o)install/$(install_disk)/disk.qcow2
	@rm -rf $$@ && mkdir -p $$(@D)
	$(QEMU_IMG) create -f qcow2 -F qcow2 -b $$(realpath --relative-to=$$(@D) $$<) $$@

.PRECIOUS: $(bigdata_dimg_o)run/$(node)/disk.qcow2
$(bigdata_dimg_o)run/$(node)/disk.qcow2: $(bigdata_dimg_o)setup/$(node)/disk.qcow2
	@rm -rf $$@ && mkdir -p $$(@D)
	$(QEMU_IMG) create -f qcow2 -F qcow2 -b $$(realpath --relative-to=$$(@D) $$<) $$@

.PHONY: qemu-bigdata-setup/$(node)
qemu-bigdata-setup/$(node): $(bigdata_dimg_o)setup/$(node)/disk.qcow2 $(d)scripts/setup_$(node).sh $(host_config_deps) $(myqemu)
	sudo -E $(myqemu) -machine q35,accel=kvm -cpu host -smp 8 -m 16G \
	-drive file=$$(word 1, $$^),media=disk,format=qcow2,if=ide,index=0 \
	-drive file=$${word 2, $$^},media=disk,format=raw,if=ide,index=1 \
	-netdev bridge,id=net-management,br=$$(MANAGEMENT_BRIDGE) \
	-device virtio-net-pci,netdev=net-management,mac=$(management_mac) \
	-boot c \
	-display none -serial mon:stdio
	touch $$<

.PHONY: qemu-bigdata-run/$(1)
qemu-bigdata-run/$(1): $(bigdata_dimg_o)run/$(1)/disk.qcow2 $(myqemu)
	sudo -E $(myqemu) -machine q35,accel=kvm -cpu host -smp 8 -m 16G \
	-drive file=$$(word 1, $$^),media=disk,format=qcow2,if=virtio,index=0 \
	-netdev bridge,id=net-management,br=$$(MANAGEMENT_BRIDGE) \
	-device virtio-net-pci,netdev=net-management,mac=$(management_mac) \
	-boot c \
	-display none -serial mon:stdio
endef

#>>> Rules for nodes

.PRECIOUS: $(bigdata_dimg_o)install/controller/disk.qcow2
$(bigdata_dimg_o)install/controller/disk.qcow2: $(bigdata_dimg_o)base/disk.qcow2 $(d)scripts/install_controller.sh $(extend_noinput_hcl) $(packer) $(platform_config_deps)
	rm -rf $(@D)
	$(packer_run) build \
	-var "base_img=$(word 1,$^)" \
	-var "disk_size=40G" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "install_script=$(word 2,$^)" \
	-var "use_backing_file=false" \
	$(extend_noinput_hcl)

.PRECIOUS: $(bigdata_dimg_o)install/worker/disk.qcow2
$(bigdata_dimg_o)install/worker/disk.qcow2: $(bigdata_dimg_o)base/disk.qcow2 $(d)scripts/install_worker.sh $(extend_noinput_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "base_img=$(word 1,$^)" \
	-var "disk_size=40G" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "install_script=$(word 2,$^)" \
	-var "use_backing_file=false" \
	$(extend_noinput_hcl)

$(eval $(call bigdata_node_rule,controller,controller,0))
$(eval $(call bigdata_node_rule,worker0,worker,1))
$(eval $(call bigdata_node_rule,worker1,worker,2))
