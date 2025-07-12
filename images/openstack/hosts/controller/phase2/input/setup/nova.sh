#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

cat <<EOF | sudo mysql -u root
CREATE DATABASE nova_api;
CREATE DATABASE nova;
CREATE DATABASE nova_cell0;
GRANT ALL PRIVILEGES ON nova_api.* TO 'nova'@'localhost' \
  IDENTIFIED BY '${NOVA_DBPASS}';
GRANT ALL PRIVILEGES ON nova_api.* TO 'nova'@'%' \
  IDENTIFIED BY '${NOVA_DBPASS}';
GRANT ALL PRIVILEGES ON nova.* TO 'nova'@'localhost' \
  IDENTIFIED BY '${NOVA_DBPASS}';
GRANT ALL PRIVILEGES ON nova.* TO 'nova'@'%' \
  IDENTIFIED BY '${NOVA_DBPASS}';
GRANT ALL PRIVILEGES ON nova_cell0.* TO 'nova'@'localhost' \
  IDENTIFIED BY '${NOVA_DBPASS}';
GRANT ALL PRIVILEGES ON nova_cell0.* TO 'nova'@'%' \
  IDENTIFIED BY '${NOVA_DBPASS}';
EOF

. ~/env/admin_openrc

openstack user create --domain default --password ${NOVA_PASS} nova
openstack role add --project service --user nova admin
openstack service create --name nova \
  --description "OpenStack Compute" compute

openstack endpoint create --region RegionOne \
  compute public http://controller:8774/v2.1
openstack endpoint create --region RegionOne \
  compute internal http://controller:8774/v2.1
openstack endpoint create --region RegionOne \
  compute admin http://controller:8774/v2.1

sudo tee /etc/nova/nova.conf < nova.conf > /dev/null

sudo su -s /bin/sh -c "nova-manage api_db sync" nova
sudo su -s /bin/sh -c "nova-manage cell_v2 map_cell0" nova
sudo su -s /bin/sh -c "nova-manage cell_v2 create_cell --name=cell1 --verbose" nova
sudo su -s /bin/sh -c "nova-manage db sync" nova

sudo systemctl restart \
  nova-api nova-scheduler nova-conductor nova-novncproxy

# Verify
sudo su -s /bin/sh -c "nova-manage cell_v2 list_cells" nova

popd
