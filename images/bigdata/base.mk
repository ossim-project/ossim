.PRECIOUS: $(bigdata_dimg_o)install/base/disk.qcow2
$(bigdata_dimg_o)install/base/disk.qcow2: $(d)input $(b)seed.raw $(d)install_base.sh $(platform_config_deps) $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=40G" \
	-var "iso_url=$(call conffget,platform,.ubuntu.disks.base.iso_url)" \
	-var "iso_cksum_url=$(call conffget,platform,.ubuntu.disks.base.iso_cksum_url)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "seedimg=$(word 2,$^)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "input_dir=$(word 1,$^)" \
	-var "install_script=$(word 3,$^)" \
	$(base_hcl)

$(b)seed.raw: $(d)user-data $(b)meta-data
	@mkdir -p $(@D)
	cloud-localds $@ $^

$(b)meta-data:
	@mkdir -p $(@D)
	tee $@ < /dev/null > /dev/null
