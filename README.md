# Ossim: Operating Support for Cluster-Scale Full-Stack Simulation

Ossim is an OS-level approach to cluster-scale full-stack simulation built on
the Linux virtualization stack. It combines full-stack fidelity for unmodified
production software with the simulation performance needed for iterative
configuration exploration.

## Setup

### Supported host

The current development and test environment is:

- Ubuntu 24.04/26.04 LTS
- x86-64 with hardware virtualization enabled and `/dev/kvm` available
- a user account that can run `sudo`

Other Linux distributions and Ubuntu releases may work, but are not currently
tested.

### Get the sources

**Source release:** extract the release tarball and enter its directory. Replace
`ossim-0123456` with the name of your archive, without the `.tar.gz` suffix:

```sh
tar -xzf ossim-0123456.tar.gz
cd ossim-0123456
```

The release includes `kernel/`, `libossim/`, `qemu/`, `ns-3/`, `workloads/`, and
their nested submodule sources. It contains no Git metadata, so skip the steps
marked **Git checkouts only** below. The dependency installation, environment
setup, and build commands apply to both source releases and Git checkouts.

**Git checkout:** clone the repository and enter it:

```sh
git clone https://github.com/ossim-project/ossim.git
cd ossim
```

Initialize the components you need using the submodule commands below.
Run the following setup and build commands from the source root: the directory
you just extracted or cloned.

### Quick start: kernel smoke test

The shortest test path builds the Ossim kernel and boots it with virtme-ng. It
does not install a kernel on the host or build the full QEMU/libossim stack.

Install the system dependencies and grant the current user KVM access:

```sh
bash scripts/install_apt_deps.sh
sudo adduser "$USER" kvm
```

Log out and back in after changing group membership. Then, from the source
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
```

**Git checkouts only:** initialize the kernel sources. Source releases already
include them.

```sh
git submodule update --init --recursive --depth 1 kernel
```

Build and run the kernel smoke test:

```sh
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

## Source release

The following packaging commands require a Git checkout. If you are building
from an existing source release, follow [Setup](#setup) and skip this section.

From a Git checkout, create a source tarball containing the current committed
`HEAD` and all nested submodules at their recorded commits:

```sh
make release
```

This writes `../ossim-<short-commit>.tar.gz` and prints its SHA-256 checksum.
Packaging requires a clean worktree, including initialized submodules: staged,
modified, and untracked files block it; Git-ignored files do not. It uses a
temporary clone with history limited to depth 1 for the main repository and all
nested submodules. After the cleanliness check, missing submodules in the current
checkout are initialized recursively with depth 1 from their configured URLs.
The release is then assembled from shallow clones of local Git repositories.
When packaging an older revision, its pinned submodule commits must also be
available locally. Git metadata is excluded. No build environment variables are
required.

To package a specific tag or commit and choose an output directory:

```sh
make release OSSIM_RELEASE_REF=v0.1.0 OSSIM_RELEASE_DIR=/path/to/releases
```

The script can also be run directly as
`bash scripts/make_release.sh [ref [output-directory]]`.

## Ossim Kernel

`kernel/` contains the Ossim custom Linux kernel and is included in source
releases.

**Git checkouts only:** initialize the kernel submodule first:

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
export OSSIM_TARGET_DIR=<source-path-on-target>
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

**Git checkouts only:** initialize the library sources first:

```sh
git submodule update --init --recursive libossim
```

For both source releases and Git checkouts, build and install:

```sh
# Build all components
make libossim

# Install to $OSSIM_PREFIX
make install-libossim

# Use ossimctl
ossimctl --help
```

## QEMU with Ossim Integration

`qemu/` contains a fork of QEMU with Ossim integration.

**Git checkouts only:** initialize the QEMU sources first:

```sh
git submodule update --init --recursive qemu
```

For both source releases and Git checkouts, build and install to `$OSSIM_PREFIX`:

```sh
# Configure QEMU with Ossim default configuration
make configure-qemu

# Build and install QEMU to $OSSIM_PREFIX
make install-qemu
```


## Run example workloads

Source releases already include `workloads/`.

**Git checkouts only:** initialize the workloads submodule:

```sh
git submodule update --init --recursive workloads
```

Refer to the instructions in `workloads/README.md` to run the example workloads.

## License

Copyright 2026 The Ossim Project.

Files maintained directly in this superproject are licensed under the Apache
License, Version 2.0. See [LICENSE](LICENSE). Each Git submodule is a separate
project distributed under its own license; the superproject license does not
relicense submodule content.

| Component | Path | License |
| --- | --- | --- |
| Ossim superproject | `.` | [Apache-2.0](LICENSE) |
| Linux kernel | `kernel/` | [GPL-2.0-only overall, with file-specific licenses and exceptions](kernel/COPYING) |
| libossim | `libossim/` | [Apache-2.0](libossim/LICENSE) |
| QEMU | `qemu/` | [GPL version 2 overall, with file-specific compatible licenses](qemu/LICENSE) |
| ns-3 | `ns-3/` | [Predominantly GPL-2.0-only, with file-specific compatible licenses](ns-3/LICENSE) |
| Workloads | `workloads/` | [GPL-2.0-only](workloads/LICENSE) |
