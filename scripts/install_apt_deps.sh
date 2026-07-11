#!/bin/bash

KERNEL_DEPS=(
    build-essential
    libncurses-dev
    bison
    flex
    libssl-dev
    libelf-dev
    libdw-dev
    fakeroot
    bc
    dwarves
    lld
    virtme-ng
    gdb
)

MISC_DEPS=(
    wget
    git
    qemu-system-x86
    clang-format
    bear
)


sudo apt-get update && sudo apt-get install -y \
    ${KERNEL_DEPS[@]} \
    ${MISC_DEPS[@]} \
    cmake \
    clang llvm pkg-config libelf-dev \
    protobuf-compiler libprotobuf-dev libseccomp-dev \
    libgrpc-dev libgrpc++-dev protobuf-compiler-grpc \
    libglib2.0-dev libfdt-dev libpixman-1-dev \
    zlib1g-dev ninja-build \
    guestfish cloud-image-utils \
    libfuse3-dev libcap-ng-dev \
    meson ninja-build \
    xsltproc libxslt1-dev libgnutls28-dev \
    python3-docutils libjson-c-dev \
    libslirp-dev \
    python3-venv python3-pip python3-setuptools \
    libnuma-dev \
    rustup \
    unzip \
    autoconf libtool

rustup default stable
