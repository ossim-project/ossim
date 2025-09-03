#!/bin/bash

if [ -z $BRIDGE_IF ] || [ -z $BRIDGE_IF_CIDR ]; then
  echo "Please set BRIDGE_IF and BRIDGE_IF_CIDR"
  exit 1
fi

if [ -z ${OSSIM_PREFIX} ]; then
  echo "Please set OSSIM_PREFIX"
  exit 1
fi

case $1 in
  setup)
    sudo ip link del ${BRIDGE_IF} >/dev/null 2>&1 || true
    sudo ip link add name ${BRIDGE_IF} type bridge
    sudo ip addr add ${BRIDGE_IF_CIDR} brd + dev ${BRIDGE_IF}
    sudo ip link set ${BRIDGE_IF} up
    sudo mkdir -p ${OSSIM_PREFIX}/etc/qemu/
    echo "allow ${BRIDGE_IF}" | sudo tee -a ${OSSIM_PREFIX}/etc/qemu/bridge.conf
    ;;
  cleanup)
    sudo ip route flush dev ${BRIDGE_IF}
    sudo ip link del ${BRIDGE_IF} || true
    ;;
  *)
    echo "Usage: $0 setup|cleanup"
    exit 1
    ;;
esac