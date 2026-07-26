# Ossim: OS-Driven Live Simulation for Cluster-Scale Full-Stack Evaluation

Ossim is an OS-level approach to cluster-scale full-stack simulation built on
the Linux virtualization stack. It combines full-stack fidelity for unmodified
production software with the simulation performance needed for iterative
configuration exploration.

Ossim coordinates live and modeled components under shared simulated time while
controlling interference among co-located live hosts. Its design comprises four
subsystems:

- **Simulation-oriented scheduling** coordinates live and modeled components
  under shared virtual time.
- **Live memory hierarchy management** controls interference among co-located
  live components.
- **Simulation-aware IPC** delivers cross-component events under virtual time.
- **Distributed simulation orchestration** composes the per-host mechanisms
  across machines for cluster-scale simulation.

Together, these mechanisms explore *simulation-native OS support*, where
simulation control and orchestration become core operating-system
responsibilities.

## Setup

### Supported host

The current development and test environment is:

- Ubuntu 26.04 LTS
- x86-64 with hardware virtualization enabled and `/dev/kvm` available
- a user account that can run `sudo`

Other Linux distributions and Ubuntu releases may work, but are not currently
tested.

### Quick start: kernel smoke test

The shortest test path builds the Ossim kernel and boots it with virtme-ng. It
does not install a kernel on the host or build the full QEMU/libossim stack.

Install the system dependencies and grant the current user KVM access:

```sh
bash scripts/install_apt_deps.sh
sudo adduser "$USER" kvm
```

Log out and back in after changing group membership. Then, from the repository
root, choose writable locations for the install, build, and output trees. These
generic values are suitable for a disposable local build and may be changed:

```sh
export OSSIM_PREFIX="${OSSIM_PREFIX:-$HOME/.local/ossim}"
export OSSIM_BUILD_DIR="${OSSIM_BUILD_DIR:-$PWD/build}"
export OSSIM_OUT_DIR="${OSSIM_OUT_DIR:-$PWD/out}"

export PATH="$OSSIM_PREFIX/bin${PATH:+:$PATH}"
export LD_LIBRARY_PATH="$OSSIM_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$OSSIM_PREFIX/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
export CPATH="$OSSIM_PREFIX/include${CPATH:+:$CPATH}"
export PKG_CONFIG_PATH="$OSSIM_PREFIX/lib/pkgconfig:$OSSIM_PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="$OSSIM_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"

git submodule update --init --recursive --depth 1 kernel
make configure-vng-kernel
make vng-kernel
make VNG_CMD="uname -r" exec-vng
```

The final command boots the freshly built kernel, prints its release, and exits.
The release should contain `-ossim`.

### Environment

The local build requires the following environment variables. Set each one to a
path appropriate for your system:

- `OSSIM_PREFIX`: installation prefix for Ossim binaries, libraries, and headers
- `OSSIM_BUILD_DIR`: out-of-tree build directory
- `OSSIM_OUT_DIR`: output and install-staging directory

Add the installation prefix to the relevant search paths:

```sh
export PATH="$OSSIM_PREFIX/bin${PATH:+:$PATH}"
export LD_LIBRARY_PATH="$OSSIM_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LIBRARY_PATH="$OSSIM_PREFIX/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
export CPATH="$OSSIM_PREFIX/include${CPATH:+:$CPATH}"
export PKG_CONFIG_PATH="$OSSIM_PREFIX/lib/pkgconfig:$OSSIM_PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="$OSSIM_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
```

See [`docs/environment.md`](docs/environment.md) for the complete configuration reference.

### Dependencies

Install the Ubuntu packages used by the kernel, QEMU, libossim, and development
tools:

```sh
bash scripts/install_apt_deps.sh
```

### KVM access

Add the current user to the `kvm` group, then log out and back in so the new
group membership takes effect:

```sh
sudo adduser "$USER" kvm
```

Verify the host setup after logging back in:

```sh
test -r /dev/kvm && test -w /dev/kvm
vng --version
```

## Ossim Kernel

`kernel/` contains the Ossim custom Linux kernel. Initialize the submodule first:

```sh
git submodule update --init --recursive --depth 1 kernel
```

### Local Kernel (Host Installation)

Build and install the kernel to the host system for full hardware testing:

**Important:** The `configure-local-kernel` target uses `/boot/config-$(uname -r)` as the base configuration by default. If you are already booted into the Ossim kernel, this will use the previous Ossim kernel config instead of your original distro kernel config. To use a specific config file, override `HOST_KERNEL_CONFIG`:

```sh
make HOST_KERNEL_CONFIG=<path to your kernel config> configure-local-kernel
```

```sh
# Configure kernel using host config
make configure-local-kernel

# Build kernel
make local-kernel

# Install kernel to the host system
make install-local-kernel

# Or build, install modules, and install the kernel in one step
make install-local-kernel-all
```

### Switching Kernels with kexec

After installing a local Ossim kernel, you can switch to it without going
through GRUB:

```sh
# Switch to the locally built/installed Ossim kernel
make OSSIM_KEXEC_KERNEL_CMDLINE="ossim_cpus=4-7" kexec-local-kernel

# Switch back to an installed distro/default kernel
make \
  OSSIM_KEXEC_DEFAULT_KERNEL="<kernel-release>" \
  OSSIM_KEXEC_DEFAULT_KERNEL_CMDLINE="<kernel-command-line>" \
  kexec-default-kernel
```

If the command-line variable is omitted, the kexec helper reuses the current
kernel command line.

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

By default, VNG boots with 8 vCPUs, 8 GiB of memory, a writable host filesystem,
and `ossim_cpus=4-7` appended to the kernel command line. Override these with
`VNG_CPUS`, `VNG_MEM`, `VNG_RW`, and `VNG_KERNEL_CMDLINE_APPEND`:

```sh
make VNG_CPUS=4 VNG_MEM=4G VNG_KERNEL_CMDLINE_APPEND="ossim_cpus=2-3" run-vng
```

For kernel debugging, use the QEMU gdbstub targets:

```sh
# Start with the gdbstub on localhost:1234
make DEBUG=1 run-vng-gdb

# Start paused at reset, then attach gdb separately
make DEBUG=1 run-vng-gdb-paused
make DEBUG=1 gdb-vng
```

#### Persistent vng Instance for Development

For continuous development and testing, run a persistent vng instance with SSH access via TCP:

```sh
# Start persistent vng instance with SSH via TCP (default port 12222)
make start-vng

# Check status
make vng-status

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

### Running make targets on a remote/lab host

Any local make target can be dispatched to a target host as `target-<goal>`.
This is useful for builds and kernel installs that should run on a lab machine
instead of the development host:

```sh
export OSSIM_TARGET_LOGIN=<user@host>
export OSSIM_TARGET_DIR=<repo-path-on-target>
export OSSIM_TARGET_SYNC=1   # set to 0 if the target tree is already up to date

# Configure/build/install on the target host
make target-configure-vng-kernel
make target-vng-kernel
make target-install-libossim
make target-install-qemu

# Switch the target host to the installed local Ossim kernel
make target-kexec-local-kernel OSSIM_KEXEC_KERNEL_CMDLINE="ossim_cpus=4-7"
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
