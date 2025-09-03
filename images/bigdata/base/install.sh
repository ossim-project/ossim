#!/bin/bash
set -eux

OUTPUT_D=/root/output/

# These groups will only take effect from the next login
usermod -aG sudo $USER
usermod -aG disk $USER
usermod -aG kvm $USER


export DEBIAN_FRONTEND=noninteractive

apt-get update && apt-get install -y \
  qemu-guest-agent
  

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt

pushd /mnt

cp ssh/* ~/.ssh
chown -R $USER:$USER ~/.ssh
chmod 600 ~/.ssh/id_rsa
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys

GRUB_CFG_FILE=/etc/default/grub.d/50-cloudimg-settings.cfg
echo 'GRUB_DISABLE_OS_PROBER=true' >> $GRUB_CFG_FILE
echo 'GRUB_HIDDEN_TIMEOUT=0' >> $GRUB_CFG_FILE
echo 'GRUB_TIMEOUT=0' >> $GRUB_CFG_FILE
update-grub

popd
umount /mnt

mkdir -p $OUTPUT_D
cp /boot/vmlinuz-$(uname -r) ${OUTPUT_D}vmlinux
cp /boot/initrd.img-$(uname -r) ${OUTPUT_D}initrd.img
cp /boot/config-$(uname -r) ${OUTPUT_D}config
