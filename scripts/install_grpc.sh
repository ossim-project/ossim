#!/bin/bash
PREFIX=${OSSIM_PREFIX:-/usr/local/ossim}
echo "Using prefix: ${PREFIX}"

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