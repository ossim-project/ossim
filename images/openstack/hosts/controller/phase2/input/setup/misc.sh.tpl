#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

. ~/env/admin_openrc

# Provider network
openstack network create  --share --external \
  --provider-physical-network provider \
  --provider-network-type flat provider
openstack subnet create --network provider \
  --allocation-pool start={{ .openstack.network.provider.ip_pool.start }},end={{ .openstack.network.provider.ip_pool.end }} \
  --dns-nameserver {{ .openstack.network.provider.nameserver }} --gateway {{ .openstack.network.provider.gateway }} \
  --subnet-range {{ .openstack.network.provider.subnet }}/{{ .openstack.network.provider.mask_len }} provider

# According to https://bugs.launchpad.net/nova/+bug/2051907, neutron policy for create_port_binding requires `service` role
openstack role add --project service --user neutron service

# For migration
sudo chsh -s /bin/bash nova
# When UsePAM is disabled for SSH, the server will refuse public key authentication
# for a disabled user (e.g., without a password set).
echo nova:nova | sudo chpasswd

. ~/env/user_openrc

# Add the keypair
openstack keypair create --public-key ~/.ssh/id_rsa.pub mykey

# Create security groups
openstack security group create mygroup
openstack security group rule create mygroup --proto any --ingress

popd
