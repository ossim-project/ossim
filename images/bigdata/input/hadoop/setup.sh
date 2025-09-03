#!/bin/bash
set -exo pipefail

pushd $(dirname ${BASH_SOURCE[0]})

source ../env.sh

SSH_DIR=../ssh
USER=hadoop

groupadd -r $USER
usermod -aG $USER root
useradd -r -m -s /bin/bash -g $USER $USER
echo "${USER}:${USER}" | chpasswd

chown -R $USER:$USER $HADOOP_HOME

# These groups will only take effect from the next login
usermod -aG sudo $USER
usermod -aG disk $USER
usermod -aG kvm $USER

echo "$USER ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USER 
chmod 600 /etc/sudoers.d/$USER

USER_HOME="/home/${USER}"
mkdir ${USER_HOME}/.ssh
cp ${SSH_DIR}/* ${USER_HOME}/.ssh
cat ${USER_HOME}/.ssh/id_rsa.pub >> ${USER_HOME}/.ssh/authorized_keys
chmod 600 ${USER_HOME}/.ssh/id_rsa
chown -R ${USER}:${USER} ${USER_HOME}/.ssh

mkdir -p /data/hdfs/namenode
mkdir -p /data/hdfs/datanode
chown -R ${USER}:${USER} /data/hdfs
chmod -R 700 /data/hdfs/namenode
chmod -R 700 /data/hdfs/datanode

mkdir -p /var/log/hadoop/{hdfs,yarn,mapred}
chown root:${USER} /var/log/hadoop
chown ${USER}:${USER} /var/log/hadoop/{hdfs,yarn,mapred}
chmod -R 775 /var/log/hadoop

chown -R root:${USER} ${HADOOP_HOME}/etc/hadoop
sudo -u root -g ${USER} cp conf/* ${HADOOP_HOME}/etc/hadoop
sudo -u $USER ${HADOOP_HOME}/bin/hdfs namenode -format

popd
