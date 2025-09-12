set -euxo pipefail

HOSTNAME=worker1

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
pushd /mnt

hostnamectl set-hostname $HOSTNAME

install -m 600 netplan/${HOSTNAME}.yaml /etc/netplan/99-netplan-config.yaml
sudo netplan apply
