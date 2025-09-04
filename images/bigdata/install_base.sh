set -euxo pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update && apt-get install -y \
  qemu-guest-agent \
  openjdk-17-jdk

# ---------- Versions ----------
HADOOP_VER=3.4.1
SPARK_VER=4.0.0
HIVE_VER=4.0.1
HBASE_VER=2.5.12
FLINK_VER=2.1.0

SBT_VER=1.10.10

# ---------- Paths ----------
INSTALL=/opt
JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk-amd64}

# ---------- Preconditions ----------
command -v curl >/dev/null || apt-get update && apt-get install -y curl
command -v java >/dev/null || apt-get install -y openjdk-17-jdk

mkdir -p $INSTALL && cd $INSTALL

# ---------- Download & unpack ----------
# Hadoop
curl -L -O https://dlcdn.apache.org/hadoop/common/hadoop-${HADOOP_VER}/hadoop-${HADOOP_VER}.tar.gz
tar -xzf hadoop-${HADOOP_VER}.tar.gz && ln -sfn hadoop-${HADOOP_VER} hadoop

# Spark (prebuilt with Hadoop3)
curl -L -O https://dlcdn.apache.org/spark/spark-${SPARK_VER}/spark-${SPARK_VER}-bin-hadoop3.tgz
tar -xzf spark-${SPARK_VER}-bin-hadoop3.tgz && ln -sfn spark-${SPARK_VER}-bin-hadoop3 spark

# Hive
curl -L -O https://dlcdn.apache.org/hive/hive-${HIVE_VER}/apache-hive-${HIVE_VER}-bin.tar.gz
tar -xzf apache-hive-${HIVE_VER}-bin.tar.gz && ln -sfn apache-hive-${HIVE_VER}-bin hive

# HBase
curl -L -O https://dlcdn.apache.org/hbase/${HBASE_VER}/hbase-${HBASE_VER}-hadoop3-bin.tar.gz || \
curl -L -O https://dlcdn.apache.org/hbase/${HBASE_VER}/hbase-${HBASE_VER}-bin.tar.gz
tar -xzf hbase-${HBASE_VER}-*-bin.tar.gz && ln -sfn hbase-${HBASE_VER}-hadoop3 hbase

# Flink
curl -L -O https://dlcdn.apache.org/flink/flink-${FLINK_VER}/flink-${FLINK_VER}-bin-scala_2.12.tgz
tar -xzf flink-${FLINK_VER}-bin-scala_2.12.tgz && ln -sfn flink-${FLINK_VER} flink

curl -L -O https://github.com/sbt/sbt/releases/download/v${SBT_VER}/sbt-${SBT_VER}.tgz
tar -xzf sbt-${SBT_VER}.tgz

rm *.tgz *.tar.gz
