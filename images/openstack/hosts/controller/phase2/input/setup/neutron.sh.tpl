#!/bin/bash
d=`dirname ${BASH_SOURCE[0]}`

set -xe

cat <<EOF | sudo mysql -u root
CREATE DATABASE neutron;
GRANT ALL PRIVILEGES ON neutron.* TO 'neutron'@'localhost' \
  IDENTIFIED BY '${NEUTRON_DBPASS}';
GRANT ALL PRIVILEGES ON neutron.* TO 'neutron'@'%' \
  IDENTIFIED BY '${NEUTRON_DBPASS}';
EOF

. ${HOME}/env/admin_openrc

openstack user create --domain default --password ${NEUTRON_PASS} neutron
openstack role add --project service --user neutron admin
openstack service create --name neutron \
  --description "OpenStack Networking" network

openstack endpoint create --region RegionOne \
  network public http://controller:9696
openstack endpoint create --region RegionOne \
  network internal http://controller:9696
openstack endpoint create --region RegionOne \
  network admin http://controller:9696

sudo ovs-vsctl add-br {{ .local.network.provider.bridge }}
sudo ovs-vsctl add-port {{ .local.network.provider.bridge }} {{ .local.network.provider.interface }}

sudo tee /etc/neutron/neutron.conf < ${d}/neutron/neutron.conf > /dev/null
sudo tee /etc/neutron/plugins/ml2/ml2_conf.ini < ${d}/neutron/ml2_conf.ini > /dev/null
sudo tee /etc/neutron/plugins/ml2/openvswitch_agent.ini < ${d}/neutron/openvswitch_agent.ini > /dev/null
sudo tee /etc/neutron/l3_agent.ini < ${d}/neutron/l3_agent.ini > /dev/null
sudo tee /etc/neutron/dhcp_agent.ini < ${d}/neutron/dhcp_agent.ini > /dev/null
sudo tee /etc/neutron/metadata_agent.ini < ${d}/neutron/metadata_agent.ini > /dev/null

sudo su -s /bin/sh -c "neutron-db-manage --config-file /etc/neutron/neutron.conf \
  --config-file /etc/neutron/plugins/ml2/ml2_conf.ini upgrade head" neutron

sleep 3

sudo systemctl restart ovs-iface-up

sudo systemctl restart \
  nova-api neutron-server neutron-openvswitch-agent neutron-dhcp-agent \
  neutron-metadata-agent neutron-l3-agent

sudo systemctl restart ovs-iface-up
