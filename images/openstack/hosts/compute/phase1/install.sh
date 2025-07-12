set -xe

if sudo growpart /dev/sda 1; then
  sudo resize2fs /dev/sda1
else
  echo "Root partition cannot be grown"
fi

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update && sudo -E apt-get install -y \
  sshpass net-tools \
  chrony python3-openstackclient python3-pip \
  nova-compute neutron-openvswitch-agent
