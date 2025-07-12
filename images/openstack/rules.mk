# Disk images output directory
openstack_img_o := $(o)
openstack_dimg_o := $(o)disks/
openstack_input_tar_o := $(o)input_tars/
openstack_install_script_o := $(o)install_scripts/
openstack_dimgs :=

.PRECIOUS: $(openstack_dimg_o)% $(openstack_input_tar_o)%

$(eval $(call include_rules,$(d)base/rules.mk))
$(eval $(call include_rules,$(d)hosts/rules.mk))

openstack_vmlinux := $(o)vmlinux
openstack_initrd := $(o)initrd.img
openstack_config := $(o)config

$(openstack_vmlinux): $(openstack_base_dimg)
	@mkdir -p $(@D)
	sudo $(virt_copy_out) -a $< /root/output/vmlinux $(@D)
	sudo chown $(shell id -u):$(shell id -g) $@
	touch $@

$(openstack_initrd): $(openstack_base_dimg)
	@mkdir -p $(@D)
	sudo $(virt_copy_out) -a $< /root/output/initrd.img $(@D)
	sudo chown $(shell id -u):$(shell id -g) $@
	touch $@

$(openstack_config): $(openstack_base_dimg)
	@mkdir -p $(@D)
	sudo $(virt_copy_out) -a $< /root/output/config $(@D)
	sudo chown $(shell id -u):$(shell id -g) $@
	touch $@

openstack_kernel_cmdline := earlyprintk=ttyS0 console=ttyS0 root=/dev/vda1 net.ifnames=0 rw

define qemu_openstack_rule # $1: disk name
.PHONY: qemu-openstack-$(1)
qemu-openstack-$(1): $(openstack_dimg_o)$(1)/disk.qcow2 $(openstack_vmlinux) $(openstack_initrd)
	$(qemu) -machine q35,accel=kvm -cpu host -smp 8 -m 16G \
	-kernel $(openstack_vmlinux) \
	-append "$(openstack_kernel_cmdline)" \
	-initrd $(openstack_initrd) \
	-drive file=$$(word 1, $$^),media=disk,format=qcow2,if=virtio,index=0 \
	-netdev user,id=user-net \
	-device virtio-net-pci,netdev=user-net \
	-boot c \
	-display none -serial mon:stdio
endef

$(eval $(call qemu_openstack_rule,base))


define openstack_disk_rules # $1: name, $2: disk (paths)
.PHONY: ubuntu-$(1) clean-ubuntu-$(1)
ubuntu-$(1): $(addprefix $(openstack_dimg_o),$(addsuffix /disk.qcow2,$(2)))
clean-ubuntu-$(1):
	rm -rf $(addprefix $(openstack_dimg_o),$(addsuffix /disk.qcow2,$(2)))
endef

define openstack_raw_disk_rules
.PHONY: ubuntu-raw-$(1) clean-ubuntu-raw-$(1)
ubuntu-raw-$(1): $(addprefix $(openstack_dimg_o),$(addsuffix /disk.raw,$(2)))
clean-ubuntu-raw-$(1):
	rm -rf $(addprefix $(openstack_dimg_o),$(addsuffix /disk.raw,$(2)))
endef

define openstack_setup_rule
$(eval node := $(1))
$(eval management_mac := $(call conffget,host,.qemu_mac_list[$(2)]))
$(eval provider_mac := $(call conffget,host,.qemu_mac_list[$(3)]))

.PRECIOUS: $(openstack_dimg_o)setup/$(node)/disk.qcow2
$(openstack_dimg_o)setup/$(node)/disk.qcow2: $(openstack_dimg_o)base/$(node)/disk.qcow2 $(openstack_input_tar_o)$(node)_phase2.tar $(openstack_install_script_o)$(node)_phase2.sh
	@rm -rf $$@ && mkdir -p $$(@D)
	$(QEMU_IMG) create -f qcow2 -F qcow2 -b $$(realpath --relative-to=$$(@D) $$<) $$@

.PHONY: qemu-ubuntu-setup/$(node)
qemu-ubuntu-setup/$(node): $(openstack_dimg_o)setup/$(node)/disk.qcow2 $(openstack_input_tar_o)$(node)_phase2.tar $(openstack_install_script_o)$(node)_phase2.sh $(host_config_deps) $(openstack_vmlinux) $(openstack_initrd)
	sudo -E $(qemu) -machine q35,accel=kvm -cpu host -smp 8 -m 16G \
	-kernel $(openstack_vmlinux) \
	-append "$(openstack_kernel_cmdline)" \
	-initrd $(openstack_initrd) \
	-drive file=$$(word 1, $$^),media=disk,format=qcow2,if=ide,index=0 \
	-drive file=$${word 2, $$^},media=disk,format=raw,if=ide,index=1 \
	-drive file=$${word 3, $$^},media=disk,format=raw,if=ide,index=2 \
	-netdev bridge,id=net-management,br=$$(call conffget,host,.bridges.management.name) \
	-device virtio-net-pci,netdev=net-management,mac=$(management_mac) \
	-netdev bridge,id=net-provider,br=$$(call conffget,host,.bridges.provider.name) \
	-device virtio-net-pci,netdev=net-provider,mac=$(provider_mac) \
	-boot c \
	-display none -serial mon:stdio
	touch $$<
endef

$(eval $(call openstack_setup_rule,controller,0,1))
$(eval $(call openstack_setup_rule,compute1,2,3))
$(eval $(call openstack_setup_rule,compute2,4,5))
$(eval $(call openstack_disk_rules,setup,setup/controller setup/compute1 setup/compute2))
