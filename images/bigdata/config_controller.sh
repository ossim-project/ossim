set -exo pipefail

source /etc/profile

HOSTNAME=controller

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
pushd /mnt

hostnamectl set-hostname $HOSTNAME

install -m 600 netplan/${HOSTNAME}.yaml /etc/netplan/99-netplan-config.yaml
netplan apply

export DEBIAN_FRONTEND=noninteractive
apt-get update && apt-get install -y \
  python3-pip \

pip3 install --break-system-packages pyspark