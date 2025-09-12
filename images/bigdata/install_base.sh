set -euxo pipefail

mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt

DOWNLOAD=/mnt/download/
INSTALL=/opt/
mkdir -p $INSTALL && cd $INSTALL

tar -xzf ${DOWNLOAD}sbt*.tgz
tar -xzf ${DOWNLOAD}hadoop*.tar.gz && ln -sfn hadoop* hadoop
tar -xzf ${DOWNLOAD}spark*.tgz && ln -sfn spark* spark
tar -xzf ${DOWNLOAD}apache-hive*.tar.gz && ln -sfn apache-hive* hive
tar -xzf ${DOWNLOAD}hbase*.tar.gz && ln -sfn hbase* hbase
tar -xzf ${DOWNLOAD}flink*.tgz && ln -sfn flink* flink

export DEBIAN_FRONTEND=noninteractive
apt-get update && apt-get install -y \
  qemu-guest-agent \
  openjdk-17-jdk

# ---------- Paths ----------
