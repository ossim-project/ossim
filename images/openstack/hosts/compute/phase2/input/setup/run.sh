#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

source common/run_pre.sh

sudo tee /etc/chrony/chrony.conf < chrony.conf > /dev/null
sudo systemctl restart chrony

# Bring up ovs interfaces
sudo cp sbin/setup-ovs-iface.sh /usr/local/sbin
sudo chmod +x /usr/local/sbin/setup-ovs-iface.sh
sudo cp services/ovs-iface-up.service /etc/systemd/system
sudo systemctl enable --now ovs-iface-up

# Configure libvirtd for live migration
sudo sed -i 's/^#listen_tls.*/listen_tls = 0/' /etc/libvirt/libvirtd.conf
sudo sed -i 's/^#listen_tcp.*/listen_tcp = 1/' /etc/libvirt/libvirtd.conf
sudo sed -i 's/^#auth_tcp.*/auth_tcp = "none"/' /etc/libvirt/libvirtd.conf
# Configure socket activation for libvirt
sudo systemctl disable libvirtd
sudo systemctl stop libvirtd
sudo systemctl mask libvirtd-tls.socket
sudo systemctl enable libvirtd-tcp.socket
sudo systemctl start libvirtd-tcp.socket

# Configure nova user for cold migration
sudo mkdir -p /var/lib/nova/.ssh
pushd /var/lib/nova/.ssh
sudo cp ~/.ssh/id_rsa.pub authorized_keys
sudo cp ~/.ssh/id_rsa id_rsa
sudo tee config > /dev/null <<EOF
Host *
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null
EOF
sudo chmod 600 authorized_keys id_rsa config
sudo chown -R nova:nova /var/lib/nova/.ssh
popd

# For migration
sudo chsh -s /bin/bash nova
# When UsePAM is disabled for SSH, the server will refuse public key authentication
# for a disabled user (e.g., without a password set).
echo nova:nova | sudo chpasswd

# # Wait at the barrier for the controller
set +x
echo -n "Waiting for ~/setup.barrier"
while ! test -f ~/setup.barrier 2> /dev/null; do 
    echo -n "."; sleep 3; 
done; echo
set -x

source ~/env/openstackrc

bash nova.sh
bash neutron.sh

source common/run_post.sh

popd
