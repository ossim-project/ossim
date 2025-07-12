.PRECIOUS: $(b)base_disk/disk.qcow2
$(b)base_disk/disk.qcow2: $(ubuntu_base_dimg) $(b)base/phase1/input.tar $(d)phase1/install.sh $(extend_hcl) $(packer) | $(ubuntu_dimg_o)
	rm -rf $(@D)
	$(packer_run) build \
	-var "base_img=$(word 1,$^)" \
	-var "disk_size=$(call conffget,platform,.ubuntu.disks.compute.size)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "user_name=root" \
	-var "user_password=$(call conffget,platform,.ubuntu.root.password)" \
	-var "input_tar_src=$(word 2,$^)" \
	-var "install_script=$(word 3,$^)" \
	-var "use_backing_file=true" \
	$(extend_hcl)

$(b)base/phase1/input.tar:
	rm -rf $(@D)/input
	mkdir -p $(@D)/input
	tar -C $(@D)/input -cf $@ .


define compute_node_rules

$(eval _n := $(1))
$(eval _inputd := $(b)$(1)/phase2/input/)

.PRECIOUS: $(ubuntu_dimg_o)base/$(_n)/disk.qcow2
$(ubuntu_dimg_o)base/$(_n)/disk.qcow2: $(b)base_disk/disk.qcow2 | $(ubuntu_dimg_o)base/$(_n)/
	@rm -f $$@
	$(QEMU_IMG) convert -O qcow2 $$< $$@

$(o)$(_n).yaml: $(d)$(_n).yaml.tpl $(config_deps) | $(o)
	$$(call confsed,$$<,$$@.tmp)
	$(yq) eval-all 'select(fileIndex == 0) * select(fileIndex == 1) | explode(.) ' $$@.tmp $(config_yaml) > $$@
	rm $$@.tmp
$(b)$(_n).sed: $(o)$(_n).yaml | $(b)
	$$(call yaml2sed,$$<,$$@)

$(ubuntu_install_script_o)$(_n)_phase2.sh: $(ubuntu_phase2_install_script)
	@mkdir -p $$(@D)
	cp $$< $$@

$(ubuntu_input_tar_o)$(_n)_phase2.tar: $$(addprefix $(_inputd), \
	$(ubuntu_phase2_common_input) \
	$$(addprefix setup/, run.sh chrony.conf nova.sh nova.conf nova-compute.conf neutron.sh neutron/neutron.conf neutron/openvswitch_agent.ini)) | $(b)$(1)/phase2/
	tar -C $(_inputd) -cf $$@ .
$(_inputd)%: $(d)phase2/input/%
	@mkdir -p $$(@D)
	cp $$< $$@
$(_inputd)%: $(d)../common/phase2/input/%
	@mkdir -p $$(@D)
	cp $$< $$@
$(_inputd)%: $(b)$(1).sed $(d)phase2/input/%.tpl
	@mkdir -p $$(@D)
	sed -f $$(word 1, $$^) $$(word 2, $$^) > $$@
$(_inputd)%: $(b)$(1).sed $(d)../common/phase2/input/%.tpl
	@mkdir -p $$(@D)
	sed -f $$(word 1, $$^) $$(word 2, $$^) > $$@
endef

$(eval $(call compute_node_rules,compute1))
$(eval $(call compute_node_rules,compute2))
