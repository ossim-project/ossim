#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

sudo tee /etc/nova/nova.conf < nova.conf > /dev/null
sudo tee /etc/nova/nova-compute.conf < nova-compute.conf > /dev/null

sudo systemctl restart nova-compute

# Wait for nova-compute to start and register itself with the controller
sleep 3

# Let controller discover compute hosts:
ssh controller '\
  cd && source env/admin_openrc && \
  openstack compute service list --service nova-compute && \
  sudo su -s /bin/sh -c "nova-manage cell_v2 discover_hosts --verbose" nova\
'

popd
