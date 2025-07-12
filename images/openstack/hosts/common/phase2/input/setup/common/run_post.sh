#!/bin/bash

pushd `dirname ${BASH_SOURCE[0]}`

sudo systemctl restart ovs-iface-up

# Set up provider veth interfaces
sudo cp sbin/setup-provider-veth.sh /usr/local/sbin
sudo chmod +x /usr/local/sbin/setup-provider-veth.sh
sudo cp services/provider-veth-up.service /etc/systemd/system
sudo systemctl enable --now provider-veth-up

touch ~/setup.done

popd
