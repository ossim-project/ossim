#!/bin/bash

sudo apt-get update && sudo apt-get install -y \
    libbpf-dev build-essential cmake \
    clang llvm pkg-config libelf-dev \
    protobuf-compiler libprotobuf-dev libseccomp-dev \
    git qemu-system-x86 \
    libglib2.0-dev libfdt-dev libpixman-1-dev \
    zlib1g-dev ninja-build bear \
    build-essential libncurses-dev bison flex \
    libssl-dev libelf-dev bc dwarves \
    guestfish cloud-image-utils \
    libfuse3-dev libcap-ng-dev \
    lld \
    meson ninja-build \
    xsltproc libxslt1-dev libgnutls28-dev \
    python3-docutils libjson-c-dev \
    libslirp-dev \
    python3-venv python3-pip python3-setuptools \
    libnuma-dev \
    rustup \
    unzip \
    clang-format \
    autoconf libtool

rustup default stable
