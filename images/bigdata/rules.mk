# Disk images output directory

bigdata_img_o := $(o)
bigdata_dimg_o := $(o)disks/
bigdata_dimgs :=

bigdata_empty_input_dir := $(b)empty_input/
$(bigdata_empty_input_dir):
	mkdir -p $@

define bigdata_disk_extend_rule
$(eval dst_disk := $(1))
$(eval src_disk := $(2))
$(eval install_script := $(3))
$(eval input_dir := $(if $(strip $(4)),$(4),$(bigdata_empty_input_dir)))

.PRECIOUS: $(bigdata_dimg_o)$(dst_disk)/disk.qcow2
$(bigdata_dimg_o)$(dst_disk)/disk.qcow2: $(bigdata_dimg_o)$(src_disk)/disk.qcow2 $(install_script) $(input_dir) $(extend_hcl) $(packer)
	rm -rf $$(@D)
	$$(packer_run) build \
	-var "base_img=$$(word 1,$$^)" \
	-var "disk_size=40G" \
	-var "cpus=$$(IMAGE_BUILD_CPUS)" \
	-var "memory=$$(IMAGE_BUILD_MEMORY)" \
	-var "out_dir=$$(@D)" \
	-var "out_name=$$(@F)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "install_script=$$(word 2,$$^)" \
	-var "input_dir=$$(word 3,$$^)" \
	-var "use_backing_file=true" \
	$$(extend_hcl)
endef

define bigdata_disk_back_rule
$(eval dst_disk := $(1))
$(eval src_disk := $(2))
$(eval back := $(3))

.PRECIOUS: $(bigdata_dimg_o)$(dst_disk)/disk.qcow2
$(bigdata_dimg_o)$(dst_disk)/disk.qcow2: $(bigdata_dimg_o)$(src_disk)/disk.qcow2
	@rm -rf $$@ && mkdir -p $$(@D)
	$(QEMU_IMG) create -f qcow2 -F qcow2 -b $$(realpath --relative-to=$$(@D) $$<) $$@
endef

define bigdata_disk_flatten_rule
$(eval dst_disk := $(1))
$(eval src_disk := $(2))

.PRECIOUS: $(bigdata_dimg_o)$(dst_disk)/disk.qcow2
$(bigdata_dimg_o)$(dst_disk)/disk.qcow2: $(bigdata_dimg_o)$(src_disk)/disk.qcow2
	@rm -rf $$@ && mkdir -p $$(@D)
	$(QEMU_IMG) convert -O qcow2 $$< $$@
endef

define bigdata_disk_run_rule
$(eval disk := $(1))
$(eval mac := $(2))

.PHONY: qemu-bigdata/$(disk)
qemu-bigdata/$(disk): $(bigdata_dimg_o)$(disk)/disk.qcow2 $(d)input/
	sudo -E $(myqemu) -machine q35,accel=kvm -cpu host -smp 8 -m 16G \
	-drive file=$$<,media=disk,format=qcow2,if=virtio,index=0 \
	-netdev bridge,id=net-management,br=$$(MANAGEMENT_BRIDGE) \
	-device virtio-net-pci,netdev=net-management,mac=$(mac) \
    -fsdev local,id=input_fsdev,path=$$(word 2,$$^),security_model=none,readonly=on \
	-device virtio-9p-pci,fsdev=input_fsdev,mount_tag=input_fsdev \
	-boot c \
	-display none -serial mon:stdio
endef

$(eval $(call include_rules,$(d)base.mk))

$(eval $(call bigdata_disk_extend_rule,base,install/base,$(d)config_base.sh,$(d)input/))

$(eval $(call bigdata_disk_extend_rule,config/controller,base,$(d)config_controller.sh,$(d)input/))
$(eval $(call bigdata_disk_flatten_rule,controller,config/controller))
$(eval $(call bigdata_disk_run_rule,controller,$(call conffget,host,.qemu_mac_list[0])))

$(eval $(call bigdata_disk_extend_rule,install/worker,base,$(d)config_worker.sh,$(d)input))

$(eval $(call bigdata_disk_extend_rule,config/worker1,install/worker,$(d)config_worker1.sh,$(d)input/))
$(eval $(call bigdata_disk_flatten_rule,worker1,config/worker1))
$(eval $(call bigdata_disk_run_rule,worker1,$(call conffget,host,.qemu_mac_list[2])))

$(eval $(call bigdata_disk_extend_rule,config/worker2,install/worker,$(d)config_worker2.sh,$(d)input/))
$(eval $(call bigdata_disk_flatten_rule,worker2,config/worker2))
$(eval $(call bigdata_disk_run_rule,worker2,$(call conffget,host,.qemu_mac_list[1])))

BIGDATA_DIMGS := controller worker1 worker2
BIGDATA_DIMGS := $(addprefix $(bigdata_dimg_o),$(BIGDATA_DIMGS))
BIGDATA_DIMGS := $(addsuffix /disk.qcow2,$(BIGDATA_DIMGS))

.PHONY: bigdata-dimgs
bigdata-dimgs: $(BIGDATA_DIMGS)
