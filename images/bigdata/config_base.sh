set -euxo pipefail

OUTPUT_D=/root/output/

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
pushd /mnt

install -m 644 env.sh /etc/profile.d/ossim_env.sh
source env.sh

groupadd -r hadoop
usermod -aG hadoop root

chown -R root:hadoop /opt/*

for USER in hdfs yarn mapred; do
    useradd -r -m -s /bin/bash -g hadoop $USER
    echo "${USER}:${USER}" | chpasswd

    # These groups will only take effect from the next login
    usermod -aG sudo $USER
    usermod -aG disk $USER
    usermod -aG kvm $USER

    echo "$USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USER 
    chmod 600 /etc/sudoers.d/$USER

    USER_HOME="/home/${USER}"
    mkdir ${USER_HOME}/.ssh
    cp ssh/* ${USER_HOME}/.ssh
    cat ${USER_HOME}/.ssh/id_rsa.pub >> ${USER_HOME}/.ssh/authorized_keys
    chmod 600 ${USER_HOME}/.ssh/id_rsa
    chown -R ${USER}:hadoop ${USER_HOME}/.ssh
done

sudo tee /etc/hosts < hosts > /dev/null

cp ssh/* /root/.ssh
cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys
chmod 600 /root/.ssh/id_rsa
chown -R root:root ~/.ssh

# Hadoop
mkdir -p /data/hdfs/namenode
mkdir -p /data/hdfs/datanode
chown -R hdfs:hadoop /data/hdfs
chmod -R 700 /data/hdfs/namenode
chmod -R 700 /data/hdfs/datanode

mkdir -p /var/log/hadoop/{hdfs,yarn,mapred}
chown root:hadoop /var/log/hadoop
chown -R hdfs:hadoop  /var/log/hadoop/hdfs
chown -R yarn:hadoop  /var/log/hadoop/yarn
chown -R mapred:hadoop /var/log/hadoop/mapred
chmod -R 775 /var/log/hadoop

cp hadoop/* ${HADOOP_HOME}/etc/hadoop
sudo -u hdfs -g hadoop ${HADOOP_HOME}/bin/hdfs namenode -format

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
