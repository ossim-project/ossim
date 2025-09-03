set -euxo pipefail

OUTPUT_D=/root/output/

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
pushd /mnt

install -m 644 env.sh /etc/profile.d/ossim_env.sh
source env.sh

sudo tee /etc/hosts < hosts > /dev/null

cp ssh/* /root/.ssh
cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys
chmod 600 /root/.ssh/id_rsa
chown -R root:root ~/.ssh

# Hadoop
bash hadoop/setup.sh

# Spark
bash spark/setup.sh

# GRUB
GRUB_CFG_FILE=/etc/default/grub.d/50-cloudimg-settings.cfg
echo 'GRUB_DISABLE_OS_PROBER=true' >> $GRUB_CFG_FILE
echo 'GRUB_HIDDEN_TIMEOUT=0' >> $GRUB_CFG_FILE
echo 'GRUB_TIMEOUT=0' >> $GRUB_CFG_FILE
update-grub

mkdir -p $OUTPUT_D
cp /boot/vmlinuz-$(uname -r) ${OUTPUT_D}vmlinux
cp /boot/initrd.img-$(uname -r) ${OUTPUT_D}initrd.img
cp /boot/config-$(uname -r) ${OUTPUT_D}config
