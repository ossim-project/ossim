#!/bin/bash
set -xe

pushd `dirname ${BASH_SOURCE[0]}`

source common/run_pre.sh

# Network configuration has been updated in common/run_pre.sh
sudo systemctl restart rabbitmq-server memcached etcd 

# Chrony
sudo tee /etc/chrony/chrony.conf < chrony.conf > /dev/null
sudo systemctl restart chrony

# SQL database
sudo cp mysql/99-openstack.cnf /etc/mysql/mariadb.conf.d
sudo systemctl enable mysql
sudo systemctl restart mysql

source ~/env/openstackrc

# Message queue
sudo rabbitmqctl add_user openstack $RABBIT_PASS
# Permit configuration, write, and read
sudo rabbitmqctl set_permissions openstack ".*" ".*" ".*"

# Memcached
sudo cp memcached.conf /etc/memcached.conf
sudo systemctl enable memcached
sudo systemctl restart memcached

# Etcd
sudo cp etcd /etc/default/etcd
sudo systemctl enable etcd
sudo systemctl restart etcd

bash keystone.sh
bash glance.sh
bash placement.sh
bash nova.sh
bash neutron.sh

mkdir ~/logs
for h in compute1 compute2; do
    set +x
    echo -n "Waiting for $h to be online"
    while ! ssh $h 'touch ~/setup.barrier'; do echo -n "."; sleep 3; done; echo 
    set -x
done

# Miscellaneous setup
bash misc.sh

source common/run_post.sh

popd
