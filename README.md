# Ossim: OS-Level Support for Fast End-to-End Compute Cluster Evaluation

Please check `docs/ossim.pdf` for an overview of the project's vision. 

## Set Up

Prerequisites:

- X86 machine
- KVM is available
- `sudo` is enabled for the current user

**Notes:** The current codebase is developed and tested in Ubuntu 25.10.
It is recommended to use the same Ubuntu version to work with the repo.
However, Ubuntu >= 24.04 should be generally fine.

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
    export CMAKE_PREFIX_PATH=${OSSIM_PREFIX}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}
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

`kernel/` contains the Ossim custom Linux kernel. Initialize the submodule first:

```sh
git submodule update --init --recursive --depth 1 kernel
```

### Local Kernel (Host Installation)

Build and install the kernel to the host system for full hardware testing:

**Important:** The `configure-local-kernel` target uses `/boot/config-$(uname -r)` as the base configuration by default. If you are already booted into the Ossim kernel, this will use the previous Ossim kernel config instead of your original distro kernel config. To use a specific config file, override `LOCAL_KERNEL_CONFIG`:

```sh
make LOCAL_KERNEL_CONFIG=c<path to your kernel config> configure-local-kernel
```

```sh
# Configure kernel using host config
make configure-local-kernel

# Build kernel
make local-kernel

# Install kernel to the host system
make install-local-kernel
```

#### Prevent Ossim Kernel from Becoming Default

By default, GRUB boots the newest kernel, which means the ossim kernel would become the default after installation. To prevent this, configure GRUB to use a saved default and pin the current kernel:

```sh
# Configure GRUB to use saved default (do NOT add GRUB_SAVEDEFAULT=true)
sudo sed -i 's/^GRUB_DEFAULT=.*/GRUB_DEFAULT=saved/' /etc/default/grub
sudo update-grub

# List menu entries to find the menuentry_id for your current kernel
grep "menuentry\|menuentry_id_option" /boot/grub/grub.cfg | head -30

# Pin the current kernel using its menuentry_id (the gnulinux-...-advanced-... string)
sudo grub-set-default "<menuentry_id>"

# Verify the saved entry
sudo grub-editenv list
```

**Note:** Do not enable `GRUB_SAVEDEFAULT=true`, as it would save any booted kernel (including one-time `grub-reboot` selections) as the new default.

#### Boot into the Installed Kernel Once

To test the ossim kernel without changing the default, use `grub-reboot` for a one-time boot:

```sh
# Set ossim kernel for next boot only using its menuentry_id
sudo grub-reboot "<menuentry_id>"

# Reboot into the selected kernel
sudo reboot
```

After testing, a normal reboot returns to the pinned default kernel. If the ossim kernel fails to boot, a hard reset will also return to the default.

### VNG Kernel (Development)

Use virtme-ng for fast iteration without rebooting the host. This builds a minimal kernel config and boots it in a VM with your host filesystem:

```sh
# Configure kernel with virtme-ng defaults (minimal config for fast builds)
make configure-vng-kernel

# Build kernel
make vng-kernel

# Boot kernel with virtme-ng (uses host filesystem)
make run-vng
```

#### Persistent vng Instance for Development

For continuous development and testing, run a persistent vng instance with SSH access via vsock:

```sh
# Start persistent vng instance with SSH via vsock (default CID 2025)
make start-vng

# Check status
make vng-status

# Run commands via SSH
make VNG_CMD="uname -r" run-vng

# Interactive SSH session
make ssh-vng

# Stop the instance
make stop-vng
```

**Note:** SSH key-based authentication requires standard SSH keys in `~/.ssh/id_*.pub` (e.g., `id_ed25519.pub`). If you don't have one, generate it with:

```sh
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -N ""
```

For one-off commands without persistent state, use `exec-vng`:

```sh
make VNG_CMD="dmesg | tail" exec-vng
```

## libossim

`libossim` is the Ossim control library and daemon package. It includes:
- **libossim**: C/C++ library for communicating with ossimd
- **ossimd**: User-space daemon that orchestrates the Ossim system
- **ossimctl**: Command-line interface to communicate with ossimd

To build and install:

```sh
git submodule update --init --recursive libossim

# Build all components
make libossim

# Install to $OSSIM_PREFIX
make install-libossim

# Use ossimctl
ossimctl --help
```

## QEMU with Ossim Integration

`qemu/` contains a fork of QEMU with Ossim integration. We need to build it from source and install to `$OSSIM_PREFIX`:

```sh
git submodule update --init --recursive qemu

# Configure QEMU with Ossim default configuration
make configure-qemu

# Build and install QEMU to $OSSIM_PREFIX
make install-qemu
```


## Run example workloads

Initialize the workloads submodule:

```sh
git submodule update --init --recursive workloads
```

Refer to the instructions in `workloads/README.md` to run the example workloads.
