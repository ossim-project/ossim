#!/bin/bash
set -eux

if sudo growpart /dev/sda 1; then
  sudo resize2fs /dev/sda1
else
  echo "Root partition cannot be grown"
fi

export DEBIAN_FRONTEND=noninteractive

sudo apt-get update && sudo -E apt-get install -y \
  sshpass net-tools \
  chrony python3-openstackclient python3-pip \
  mariadb-server python3-pymysql \
  rabbitmq-server \
  memcached python3-memcache \
  etcd-server \
  keystone \
  glance \
  placement-api \
  nova-api nova-conductor nova-novncproxy nova-scheduler \
  neutron-server neutron-plugin-ml2 \
  neutron-openvswitch-agent neutron-l3-agent neutron-dhcp-agent \
  neutron-metadata-agent
