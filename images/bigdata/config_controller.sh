set -eux

HOSTNAME=controller

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
pushd /mnt/nodes/$HOSTNAME


hostnamectl set-hostname $HOSTNAME

sudo install -m 600 netplan.yaml /etc/netplan/99-netplan-config.yaml
sudo netplan apply
