#/bin/bash

VETH="veth-provider"
PEER="veth-providerp"
BR={{ .local.network.provider.bridge }}

# Clean up
ovs-vsctl --if-exists del-port $BR $PEER
ip link delete $VETH type veth || true

# Set up
ip link add $VETH type veth peer name $PEER

ip addr add {{ .local.network.provider.ip }}/{{ .openstack.network.provider.mask_len }} dev $VETH
ovs-vsctl add-port $BR $PEER

ip link set $VETH up
ip link set $PEER up
