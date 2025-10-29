TEST_DIMG_ISO_URL := https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
TEST_DIMG_CKSUM_URL := https://cloud-images.ubuntu.com/noble/current/SHA256SUMS
TEST_DIMG_DISK_SIZE := 40G

TEST_VM_CPUS := 16
TEST_VM_MEMORY := 64G

test_dimg := $(o)test/disk.qcow2

.PHONY: dev-dimg
dev-dimg: $(dev_dimg)

$(test_dimg): $(b)seed.raw $(d)install.sh $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=$(TEST_DIMG_DISK_SIZE)" \
	-var "iso_url=$(TEST_DIMG_ISO_URL)" \
	-var "iso_cksum_url=$(TEST_DIMG_CKSUM_URL)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "seedimg=$(word 1,$^)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "input_dir=$(project_root)" \
	-var "install_script=$(word 2,$^)" \
	$(base_hcl)

$(b)seed.raw: $(d)user-data $(b)meta-data
	@mkdir -p $(@D)
	cloud-localds $@ $^

$(b)meta-data:
	@mkdir -p $(@D)
	tee $@ < /dev/null > /dev/null

.PHONY: myqemu-test
myqemu-test: $(test_dimg) $(TEST_VM_VIRTIOFS_SOCK)
	$(myqemu) -machine q35,accel=kvm -cpu host -smp $(TEST_VM_CPUS) -m $(TEST_VM_MEMORY) \
	-object memory-backend-memfd,id=mem0,size=$(TEST_VM_MEMORY),share=on \
	-numa node,memdev=mem0 \
	-drive file=$(test_dimg),media=disk,format=qcow2,if=virtio,index=0 \
	-netdev user,id=user-net \
	-boot c \
	-display none -serial mon:stdio

