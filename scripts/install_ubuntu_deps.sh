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

# rustup default stable

PREFIX=${OSSIM_PREFIX:-/usr/local/ossim}
echo "Using prefix: ${PREFIX}"

# yq
yq=${PREFIX}/bin/yq
mkdir -p $(dirname $(yq))
wget -qO ${yq} https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 && \
chmod a+x ${yq} && \
echo "yq installed to ${yq}"

# gRPC
GRPC_BRANCH=v1.76.0
GRPC_SRC=/tmp/grpc-${GRPC_BRANCH}
rm -rf ${GRPC_SRC}
git clone --depth 1 --recurse-submodules --shallow-submodules -b ${GRPC_BRANCH} https://github.com/grpc/grpc.git ${GRPC_SRC}

echo "Building gRPC in ${GRPC_SRC}"
mkdir -p ${GRPC_SRC}/build
pushd ${GRPC_SRC}/build
cmake -DBUILD_SHARED_LIBS=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_SSL_PROVIDER=package \
      -DgRPC_ZLIB_PROVIDER=package \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_BUILD_TYPE=Release \
      -DgRPC_INSTALL=ON \
      -DCMAKE_INSTALL_PREFIX=${PREFIX} \
      ..
make -j$(nproc)

echo "Installing gRPC to ${PREFIX}"
make install
echo "gRPC installed to ${PREFIX}"
popd




