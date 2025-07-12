#!/bin/bash
set -xe
pushd `dirname ${BASH_SOURCE[0]}`


# Create the keystone database
cat <<EOF | sudo mysql -u root
CREATE DATABASE keystone;
GRANT ALL PRIVILEGES ON keystone.* TO 'keystone'@'localhost' IDENTIFIED BY '${KEYSTONE_DBPASS}'; 
GRANT ALL PRIVILEGES ON keystone.* TO 'keystone'@'%' IDENTIFIED BY '${KEYSTONE_DBPASS}';
EOF

# Install keystone
# Edit /etc/keystone/keystone.conf
sudo tee /etc/keystone/keystone.conf < keystone.conf > /dev/null
# Populate the Identity service database
sudo su -s /bin/sh -c "keystone-manage db_sync" keystone
# Initialize Fernet key repositories
sudo keystone-manage fernet_setup --keystone-user keystone --keystone-group keystone
sudo keystone-manage credential_setup --keystone-user keystone --keystone-group keystone
# Bootstrap the Identity service
sudo keystone-manage bootstrap --bootstrap-password $ADMIN_PASS \
  --bootstrap-admin-url http://controller:5000/v3/ \
  --bootstrap-internal-url http://controller:5000/v3/ \
  --bootstrap-public-url http://controller:5000/v3/ \
  --bootstrap-region-id RegionOne

# Configure the Apache HTTP server
sudo sed -i '/^ServerName /s/^/# /; $a ServerName controller' /etc/apache2/apache2.conf
sudo systemctl restart apache2

. ~/env/admin_openrc
# The service project contains a user for each service
openstack project create --domain default --description "Service Project" service 

# Create a unprivileged project and user
openstack project create --domain default --description "Non-Admin Project" $USER_PROJECT
openstack user create --domain default --password $USER_PASS $USER_NAME
openstack role add --project $USER_PROJECT --user $USER_NAME admin

popd
