#!/bin/bash

if [ -z $BRIDGE_IF ] || [ -z $INTERNET_IF ]; then
  echo "Please set BRIDGE_IF and INTERNET_IF"
  exit 1
fi

filter_rules=(
  "FORWARD -i ${BRIDGE_IF} -o ${INTERNET_IF} -j ACCEPT"
  "FORWARD -i ${INTERNET_IF} -o ${BRIDGE_IF} -m state --state RELATED,ESTABLISHED -j ACCEPT"
)

nat_rules=(
  "POSTROUTING -o ${INTERNET_IF} -j MASQUERADE"
)

case $1 in
  setup)
    sudo sysctl net.ipv4.ip_forward=1
    for ((i=0; i<${#filter_rules[@]}; i++)); do
      sudo iptables -t filter -A ${filter_rules[i]}
    done
    for ((i=0; i<${#nat_rules[@]}; i++)); do
      sudo iptables -t nat -A ${nat_rules[i]}
    done
    ;;
  cleanup)
    for ((i=0; i<${#filter_rules[@]}; i++)); do
      sudo iptables -t filter -D ${filter_rules[i]} || true
    done
    for ((i=0; i<${#nat_rules[@]}; i++)); do
      sudo iptables -t nat -D ${nat_rules[i]} || true
    done
    ;;
  *)
    echo "Usage: $0 setup|cleanup"
    exit 1
    ;;
esac