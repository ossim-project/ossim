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
    bash scripts/install_deps.sh
    ```

    This may take more than 10 minutes because it builds some dependencies (e.g., gRPC) from source.

    Optionally, you can configure `GRPC_CXXFLAGS` and `GRPC_LDFLAGS` environment variables (e.g., also in `/etc/profile.d/ossim.sh`) to save build time (it takes some time `pkg-config` to work with gRPC).
    You can initialize the variables by copying the output from `pkg-config --cflags grpc++ protobuf` and `pkg-config --libs grpc++ protobuf` respectively.

3. Add the current user to related groups and relogin:

    ```sh
    sudo adduser $USER kvm
    ```

    You may want to re-login for the configuration to take effect.


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

