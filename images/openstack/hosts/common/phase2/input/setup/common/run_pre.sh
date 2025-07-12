#!/bin/bash

pushd `dirname ${BASH_SOURCE[0]}`

if [ -f ~/setup.done ]; then
    echo "Already set up"
    exit 0
fi

sudo tee /etc/hosts < hosts > /dev/null
sudo hostnamectl set-hostname $(cat hostname)
sudo rm -rf /etc/netplan/*
sudo install -m 600 netplan.yaml /etc/netplan/99-netplan-config.yaml
sudo netplan apply

# Bring up ovs interfaces
sudo cp sbin/setup-ovs-iface.sh /usr/local/sbin
sudo chmod +x /usr/local/sbin/setup-ovs-iface.sh
sudo cp services/ovs-iface-up.service /etc/systemd/system
sudo systemctl enable --now ovs-iface-up

popd
