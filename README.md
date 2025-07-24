# Ossim: OS-Driven End-to-End Simulation

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

2. Add the current user to related groups and relogin:

    ```sh
    sudo adduser $USER kvm
    sudo adduser $USER libvirt
    exit
    ```

3. Initialize submodules:

    ```sh
    git submodule udpate --init --depth 1 linux
    git submodule update --init --recursive --depth 1 qemu
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
