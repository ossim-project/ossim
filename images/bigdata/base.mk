HADOOP_VER=3.4.1
SPARK_VER=4.0.0
HIVE_VER=4.0.1
HBASE_VER=2.5.12
FLINK_VER=2.1.0

SBT_VER=1.10.10

download_d := $(d)input/download/
HADOOP_TGZ := $(download_d)hadoop-${HADOOP_VER}.tar.gz
SPARK_TGZ := $(download_d)spark-${SPARK_VER}-bin-hadoop3.tgz
HIVE_TGZ := $(download_d)apache-hive-${HIVE_VER}-bin.tar.gz
HBASE_TGZ := $(download_d)hbase-${HBASE_VER}-hadoop3-bin.tar.gz
FLINK_TGZ := $(download_d)flink-${FLINK_VER}-bin-scala_2.12.tgz
SBT_TGZ := $(download_d)sbt-${SBT_VER}.tgz

$(HADOOP_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://dlcdn.apache.org/hadoop/common/hadoop-${HADOOP_VER}/hadoop-${HADOOP_VER}.tar.gz

$(SPARK_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://dlcdn.apache.org/spark/spark-${SPARK_VER}/spark-${SPARK_VER}-bin-hadoop3.tgz

$(HIVE_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://dlcdn.apache.org/hive/hive-${HIVE_VER}/apache-hive-${HIVE_VER}-bin.tar.gz

$(HBASE_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://dlcdn.apache.org/hbase/${HBASE_VER}/hbase-${HBASE_VER}-hadoop3-bin.tar.gz

$(FLINK_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://dlcdn.apache.org/flink/flink-${FLINK_VER}/flink-${FLINK_VER}-bin-scala_2.12.tgz

$(SBT_TGZ):
	@mkdir -p $(@D)
	curl -L -o $@ https://github.com/sbt/sbt/releases/download/v${SBT_VER}/sbt-${SBT_VER}.tgz

bigdata_download_files := $(HADOOP_TGZ) $(SPARK_TGZ) $(HIVE_TGZ) $(HBASE_TGZ) $(FLINK_TGZ) $(SBT_TGZ)
.PRECIOUS: $(bigdata_download_files)

.PRECIOUS: $(bigdata_dimg_o)install/base/disk.qcow2
$(bigdata_dimg_o)install/base/disk.qcow2: $(d)input $(b)seed.raw $(d)install_base.sh $(bigdata_download_files) $(platform_config_deps) $(base_hcl) $(packer)
	rm -rf $(@D)
	$(packer_run) build \
	-var "disk_size=40G" \
	-var "iso_url=$(call conffget,platform,.ubuntu.disks.base.iso_url)" \
	-var "iso_cksum_url=$(call conffget,platform,.ubuntu.disks.base.iso_cksum_url)" \
	-var "out_dir=$(@D)" \
	-var "out_name=$(@F)" \
	-var "cpus=$(IMAGE_BUILD_CPUS)" \
	-var "memory=$(IMAGE_BUILD_MEMORY)" \
	-var "seedimg=$(word 2,$^)" \
	-var "user_name=root" \
	-var "user_password=root" \
	-var "input_dir=$(word 1,$^)" \
	-var "install_script=$(word 3,$^)" \
	$(base_hcl)

$(b)seed.raw: $(d)user-data $(b)meta-data
	@mkdir -p $(@D)
	cloud-localds $@ $^

$(b)meta-data:
	@mkdir -p $(@D)
	tee $@ < /dev/null > /dev/null
