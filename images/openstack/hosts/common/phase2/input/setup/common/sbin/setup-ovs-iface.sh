#!/bin/bash

ip link set ovs-system up
for bridge in $(ovs-vsctl list-br); do 
    ip link set $bridge up; 
done
