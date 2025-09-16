#!/bin/bash
set -exo pipefail

pushd $(dirname ${BASH_SOURCE[0]})

source ../env.sh

SSH_DIR=../ssh
USER=hadoop

chown -R root:${USER} $SPARK_HOME

for dir in /var/log/spark /var/lib/spark/work; do
    mkdir -p $dir
    chown -R root:$USER $dir
    chmod -R 775 $dir
done 

sudo -u root -g ${USER} cp conf/* ${SPARK_HOME}/conf

popd
