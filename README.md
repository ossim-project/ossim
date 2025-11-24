# Ossim: OS-Level Support for Fast End-to-End Compute Cluster Evaluation

Please check `docs/ossim.pdf` for an overview of the project's vision. 

## Set Up

Prerequisites:

- X86 machine with **Ubuntu 25.10**
- KVM is available
- `sudo` is enabled for the current user

You may also want to configure the following environment variables:

- `OSSIM_PREFIX`: The install directory, default: `/usr/local/ossim/`
- `OSSIM_BUILD`: The build directory, default: `./build/`
- `OSSIM_OUTPUT`: The output directory, default: `./out/`

Steps:

1. Update environment variables to include the `OSSIM_PREFIX`:
    ```sh
    export PATH=${OSSIM_PREFIX}/bin${PATH:+:$PATH}
    export LD_LIBRARY_PATH=${OSSIM_PREFIX}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    export LIBRARY_PATH=${OSSIM_PREFIX}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
    export CPATH=${OSSIM_PREFIX}/include${CPATH:+:$CPATH}
    export PKG_CONFIG_PATH=${OSSIM_PREFIX}/lib/pkgconfig:${OSSIM_PREFIX}/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}

    ```
    You may want to persist these changes in `/etc/profile.d/ossim.sh`, so that it can be also seem by `sudo -i`. (`sudo -E` is not supported by `sudo-rs` (default in Ubuntu 25.10.), as for November 2025.)

2. Install dependencies:

    ```sh
    bash scripts/install_ubuntu_deps.sh
    ```

    This may take more than 10 minutes because it builds some dependencies (e.g., gRPC) from source.

    Optionally, you can configure `GRPC_CXXFLAGS` and `GRPC_LDFLAGS` environment variables (e.g., also in `/etc/profile.d/ossim.sh`) to save build time (it takes some time `pkg-config` to work with gRPC).
    You can initialize the variables by copying the output from `pkg-config --cflags grpc++ protobuf` and `pkg-config --libs grpc++ protobuf` respectively.

3. Add the current user to related groups and relogin:

    ```sh
    sudo adduser $USER kvm
    sudo adduser $USER libvirt
    ```

    You may want to re-login for the configuration to take effect.

4. Initialize submodules:

    ```sh
    git submodule update --init --recursive --depth 1 qemu
    # git submodule update --init --depth 1 linux
    ```

5. Build and install QEMU

    ```sh
    make configure-qemu
    make install-qemu
    ```

## Run Big Data Applications

The set up assumes that subnets `10.10.10.0/24` and `10.10.11.0/24` are free.
`$INTERNET_IF` contains the name of network interface that has access to the Internet.

The example applications include:

- Spark TPC-DS 99
- Hive TPC-DS 99
- Hbase YCSB
- Flink Hibench
- MySQL OLTP/TPCC
- MySQL OLAP/TPCH

1. Set up Linux bridges

    ```sh
    make INTERNET_IF=$INTERNET_IF setup-bridges
    make INTERNET_IF=$INTERNET_IF setup-nat
    ```

2. Build disk images:

    ```sh
    make bigdata-dimgs
    ```

    The build can take 10 minutes or longer.

3. Run controller, worker1, and worker2 in three different sessions (e.g., `screen`, `tmux`, etc.):

    ```sh
    # In session 1
    make qemu-bigdata/controller
    # In session 2
    make qemu-bigdata/worker1
    # In session 3
    make qemu-bigdata/worker2
    ```

4. Run Spark TPC-DS 99:

    Login to controller:
    ```sh
    ssh hadoop@10.10.10.100 # password: hadoop
    ```

    Start Hadoop and Spark on the cluster:
    ```sh
    $HADOOP_HOME/sbin/start-dfs.sh
    $SPARK_HOME/sbin/start-all.sh
    ```

    Mount the input shared directory:
    ```sh
    sudo mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
    # Alternatively, run `sudo mount_input_fs.sh /mnt`
    ```

    Prepare TPC-DS 99 dataset:
    ```sh
    bash /mnt/tpcds/prepare_data.sh
    ```

    Run the benchmark:
    ```sh
    python3 /mnt/tpcds/spark.py
    ```
 
## Developer Guide

- Develop Linux kernel

    ```sh
    # Compile the kernel (optional)
    make build-linux

    # Prepare the disk image (optional)
    make linuxdev-dimg

    # Bootstrap the kernel on QEMU
    make qemu-linuxdev
    ```
