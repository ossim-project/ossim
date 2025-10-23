DEV_DIMG_ISO_URL := https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img
DEV_DIMG_CKSUM_URL := https://cloud-images.ubuntu.com/noble/current/SHA256SUMS
DEV_DIMG_DISK_SIZE := 40G

DEV_VM_CPUS := 16
DEV_VM_MEMORY := 64G
DEV_VM_SSH_PORT := 12222
DEV_VM_VIRTIOFS_SOCK := /tmp/ossim_dev_virtiofs.sock

DEV_VM_MOUNT_DIR := $(abspath ../$(project_root))

dev_dimg := $(o)dev/disk.qcow2

.PHONY: dev-dimg
dev-dimg: $(dev_dimg)

$(dev_dimg): $(b)seed.raw $(d)install.sh $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=$(DEV_DIMG_DISK_SIZE)" \
	-var "iso_url=$(DEV_DIMG_ISO_URL)" \
	-var "iso_cksum_url=$(DEV_DIMG_CKSUM_URL)" \
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

.PHONY: qemu-dev
qemu-dev: $(dev_dimg) $(DEV_VM_VIRTIOFS_SOCK)
	$(qemu) -machine q35,accel=kvm -cpu host -smp $(DEV_VM_CPUS) -m $(DEV_VM_MEMORY) \
	-object memory-backend-memfd,id=mem0,size=$(DEV_VM_MEMORY),share=on \
	-numa node,memdev=mem0 \
	-drive file=$(dev_dimg),media=disk,format=qcow2,if=virtio,index=0 \
	-netdev user,id=user-net,hostfwd=tcp::$(DEV_VM_SSH_PORT)-:22 \
	-device virtio-net-pci,netdev=user-net \
	-chardev socket,id=char0,path=$(DEV_VM_VIRTIOFS_SOCK) \
	-device vhost-user-fs-pci,chardev=char0,tag=share_fsdev \
	-boot c \
	-display none -serial mon:stdio

.PHONY: virtiofs-dev
virtiofs-dev:
	rm -f $(DEV_VM_VIRTIOFS_SOCK)
	/usr/libexec/virtiofsd \
		--socket-path=$(DEV_VM_VIRTIOFS_SOCK) \
		--shared-dir $(DEV_VM_MOUNT_DIR) \
		--cache always \
		--sandbox none \
		--log-level debug

