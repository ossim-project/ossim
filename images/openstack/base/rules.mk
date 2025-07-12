openstack_base_dimg := $(b)disk/disk.qcow2

.PHONY: openstack_base_dimg
openstack-base-dimg: $(openstack_base_dimg)

$(openstack_base_dimg): $(d)input $(b)seed.raw $(d)install.sh $(platform_config_deps) $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=$(call conffget,platform,.ubuntu.disks.base.size)" \
	-var "iso_url=$(call conffget,platform,.ubuntu.disks.base.iso_url)" \
	-var "iso_cksum_url=$(call conffget,platform,.ubuntu.disks.base.iso_cksum_url)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "seedimg=$(word 2,$^)" \
	-var "user_name=root" \
	-var "user_password=$(call conffget,platform,.ubuntu.root.password)" \
	-var "input_dir=$(word 1,$^)" \
	-var "install_script=$(word 3,$^)" \
	$(base_hcl)

$(openstack_dimg_o)base/disk.qcow2: $(openstack_base_dimg)
	@mkdir -p $(@D)
	$(qemu_img) create -f qcow2 -F qcow2 -b $(realpath --relative-to=$@ $<) $@	

$(b)seed.raw: $(b)user-data $(b)meta-data | $(o)
	cloud-localds $@ $^

$(b)user-data: $(d)user-data.tpl $(platform_config_deps) | $(b)
	$(call conffsed,platform,$<,$@)

$(b)meta-data: | $(b)
	tee $@ < /dev/null > /dev/null

$(d)input/: $(addprefix $(d)input/ssh/, id_rsa id_rsa.pub config)
	touch $@