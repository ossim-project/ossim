#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

# Create the glance database
cat <<EOF | sudo mysql -u root
CREATE DATABASE placement;
GRANT ALL PRIVILEGES ON placement.* TO 'placement'@'localhost' IDENTIFIED BY '${PLACEMENT_DBPASS}';
GRANT ALL PRIVILEGES ON placement.* TO 'placement'@'%' IDENTIFIED BY '${PLACEMENT_DBPASS}';
EOF

. ~/env/admin_openrc

openstack user create --domain default --password ${PLACEMENT_PASS} placement
openstack role add --project service --user placement admin
openstack service create --name placement --description "Placement API" placement

openstack endpoint create --region RegionOne placement public http://controller:8778
openstack endpoint create --region RegionOne placement internal http://controller:8778
openstack endpoint create --region RegionOne placement admin http://controller:8778

sudo tee /etc/placement/placement.conf < placement.conf > /dev/null

sudo su -s /bin/sh -c "placement-manage db sync" placement

sudo systemctl restart apache2

# Verify
sudo placement-status upgrade check
# openstack --os-placement-api-version 1.2 resource class list --sort-column name
# openstack --os-placement-api-version 1.6 trait list --sort-column name

popd
