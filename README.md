# Ossim: End-to-End Simulation with OS-Level Supports

## Quick Start

Prerequisites:

- X86 machine with Ubuntu 24.04
- KVM is available
- `sudo` is enabled for the current user

Steps:

1. Install dependencies:

```sh
make install-dependencies
```

2. Initialize submodules:

```sh
git submodule udpate --init --depth 1 linux
git submodule update --init --recursive --depth 1 qemu
```