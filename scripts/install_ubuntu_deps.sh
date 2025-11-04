sudo apt-get update && sudo apt-get install -y \
    libbpf-dev build-essential cmake \
    clang llvm pkg-config libelf-dev \
    protobuf-compiler libseccomp-dev \
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
    unzip

rustup default stable

# if define OSSIM_PREFIX then use ${OSSIM_PREFIX}/bin, otherwise ${HOME}/bin
YQ_INSTALL_DIR=${OSSIM_PREFIX:-${HOME}}/bin

mkdir -p ${YQ_INSTALL_DIR}
yq=${YQ_INSTALL_DIR}/yq

wget -qO ${yq} https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64 && \
chmod a+x ${yq} && \
echo "yq installed to ${yq}"
