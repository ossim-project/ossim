#!/bin/bash
d=`dirname ${BASH_SOURCE[0]}`

set -xe

sudo ovs-vsctl add-br {{ .local.network.provider.bridge }}
sudo ovs-vsctl add-port {{ .local.network.provider.bridge }} {{ .local.network.provider.interface }}

sudo tee /etc/neutron/neutron.conf < ${d}/neutron/neutron.conf > /dev/null
sudo tee /etc/neutron/plugins/ml2/openvswitch_agent.ini < ${d}/neutron/openvswitch_agent.ini > /dev/null

sudo systemctl restart nova-compute
sudo systemctl restart neutron-openvswitch-agent

sleep 3

# Verify
ssh controller '\
  source ~/env/admin_openrc && \
  openstack network agent list \
'
