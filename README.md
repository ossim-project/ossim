# Ossim: OS-Level Support for Fast End-to-End Compute Cluster Evaluation

Please check `docs/ossim.pdf` for an overview of the project's vision. 

## Set Up

Prerequisites:

- X86 machine with **Ubuntu 25.10**
- KVM is available
- `sudo` is enabled for the current user


Steps:

1. Configure the following environment variables:

    - `OSSIM_PREFIX`: The install directory (e.g., `/usr/local/ossim/`)
    - `OSSIM_BUILD_DIR`: The build directory, (e.g., `./build/`)
    - `OSSIM_OUT_DIR`: The output directory, (e.g, `./out/`)

    Also update environment variables to include the `OSSIM_PREFIX`:
    ```sh
    export PATH=${OSSIM_PREFIX}/bin${PATH:+:$PATH}
    export LD_LIBRARY_PATH=${OSSIM_PREFIX}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
    export LIBRARY_PATH=${OSSIM_PREFIX}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}
    export CPATH=${OSSIM_PREFIX}/include${CPATH:+:$CPATH}
    export PKG_CONFIG_PATH=${OSSIM_PREFIX}/lib/pkgconfig:${OSSIM_PREFIX}/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
    ```

2. Install dependencies:

    ```sh
    bash scripts/install_apt_deps.sh
    bash scripts/intall_grpc.sh
    ```

    This may take more than 10 minutes because it builds some dependencies (e.g., gRPC) from source.

    Optionally, you can configure `GRPC_CXXFLAGS` and `GRPC_LDFLAGS` environment variables (e.g., also in `/etc/profile.d/ossim.sh`) to save build time (it takes some time `pkg-config` to work with gRPC).
    You can initialize the variables by copying the output from `pkg-config --cflags grpc++ protobuf` and `pkg-config --libs grpc++ protobuf` respectively.

3. Add the current user to related groups and relogin:

    ```sh
    sudo adduser $USER kvm
    ```

    You may want to re-login for the configuration to take effect.


## Ossim Kernel

`kernel/` contains the Ossim custom Linux kernel. Build and test it using virtme-ng for rapid iteration:

```sh
# Configure kernel using host config
make configure-kernel
# Alternatively, use virtme-ng defaults for minimal config
make configure-kernel-vng

# Build kernel
make build-kernel -j`nproc`

# Test kernel with virtme-ng (boots with host filesystem)
make vng-kernel
```

To install the kernel to the host system:

```sh
make install-kernel 
```


## QEMU with Ossim Integration

`qemu/` contains a fork of QEMU with Ossim integration. We need to build it from source and install to `$OSSIM_PREFIX`:

```sh
git submodule update --init --recursive --depth 1 qemu

# Configure QEMU with Ossim default configuration
make configure-qemu

# Build and install QEMU to $OSSIM_PREFIX
make install-qemu
```


## Ossim Daemon (`ossimd`)

`ossimd` is a user-space daemon that orchestrates the Ossim system. It loads and manages the Ossim SCX scheduler and serves as an gRPC server to communicate with Ossim QEMU instances.  

To build and run `ossimd`:

```sh
make ossimd
make run-ossimd
```

## Ossim Control CLI (`ossimctl`)

`ossimctl` is a command-line interface to communicate with `ossimd`.

To build, install, and run `ossimctl`:
```sh
# This will build and install ossimctl to $OSSIM_PREFIX
make install-ossimctl

ossimctl --help
```

## Run example workloads

Please clone the [workloads repository](https://github.com/ossim-project/workloads) and follow the instructions in the README.

