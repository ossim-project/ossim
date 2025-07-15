#!/bin/bash
set -eux

BUILD_LINUX=0

LINUX_D=/root/linux/
OUTPUT_D=/root/output/

# These groups will only take effect from the next login
usermod -aG sudo $USER
usermod -aG disk $USER
usermod -aG kvm $USER

if [ $BUILD_LINUX -eq 1 ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update && apt-get install -y \
    build-essential \
    libncurses-dev \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    bc \
    dwarves \
    qemu-guest-agent \
    rsync \
    lld

    # Build and install the kernel
    mount -t 9p -o trans=virtio,ro,cache=loose input_fsdev /mnt
    mkdir -p $LINUX_D
    rsync -vr --exclude='.git' /mnt/ $LINUX_D
    umount /mnt

    pushd $LINUX_D
    cp /boot/config-$(uname -r) .config

    ./scripts/config
        --disable CONFIG_MODVERSIONS

    ./scripts/config \
        --disable CONFIG_MODULE_SIG \
        --disable CONFIG_MODULE_SIG_ALL \
        --set-str CONFIG_SYSTEM_TRUSTED_KEYS "" \
        --set-str CONFIG_SYSTEM_REVOCATION_KEYS "" \
        --set-str CONFIG_MODULE_SIG_KEY ""

    ./scripts/config \
        --disable CONFIG_WIRELESS \
        --disable CONFIG_WLAN \
        --disable CONFIG_CFG80211 \
        --disable CONFIG_MAC80211 \
        --disable CONFIG_IWLWIFI \
        --disable CONFIG_BT \
        --disable CONFIG_IEEE802154 \
        --disable CONFIG_NET_VENDOR_MELLANOX \
        --disable CONFIG_MELLANOX_PLATFORM \
        --disable CONFIG_INFINIBAND \
        --disable CONFIG_COMEDI \
        --disable CONFIG_IIO \
        --disable CONFIG_I2C \
        --disable CONFIG_SPI \
        --disable CONFIG_GPIO \
        --disable CONFIG_HID \
        --disable CONFIG_MEDIA_SUPPORT \
        --disable CONFIG_SOUND \
        --disable CONFIG_INPUT_MOUSE \
        --disable CONFIG_INPUT_JOYSTICK \
        --disable CONFIG_INPUT_TABLET \
        --disable CONFIG_INPUT_TOUCHSCREEN \
        --disable CONFIG_INPUT_MISC \
        --disable CONFIG_HID_SUPPORT \
        --disable CONFIG_HID \
        --disable CONFIG_DRM \
        --disable CONFIG_DRM_AMDGPU \
        --disable CONFIG_DRM_VIRTIO_GPU \
        --disable CONFIG_FB

    make olddefconfig

    make LD=ld.lld -j$(nproc)
    make modules_install
    make install

    KERNEL_RELEASE=$(cat include/config/kernel.release)

    popd # $LINUX_D
else
    KERNEL_RELEASE=$(uname -r)
fi

mkdir -p $OUTPUT_D
echo $KERNEL_RELEASE > ${OUTPUT_D}release
cp /boot/vmlinuz-$KERNEL_RELEASE ${OUTPUT_D}vmlinux
cp /boot/initrd.img-$KERNEL_RELEASE ${OUTPUT_D}initrd.img
cp /boot/config-$KERNEL_RELEASE ${OUTPUT_D}config

GRUB_CFG_FILE=/etc/default/grub.d/50-cloudimg-settings.cfg
echo 'GRUB_DISABLE_OS_PROBER=true' >> $GRUB_CFG_FILE
echo 'GRUB_HIDDEN_TIMEOUT=0' >> $GRUB_CFG_FILE
echo 'GRUB_TIMEOUT=0' >> $GRUB_CFG_FILE
update-grub
