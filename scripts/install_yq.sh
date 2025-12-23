#!/bin/bash

PREFIX=${OSSIM_PREFIX:-/usr/local/ossim}
echo "Using prefix: ${PREFIX}"

# yq
yq=${PREFIX}/bin/yq
mkdir -p $(dirname $(yq))
wget -qO ${yq} https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 && \
chmod a+x ${yq} && \
echo "yq installed to ${yq}"
