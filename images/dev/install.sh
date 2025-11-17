set -eux

cat > /usr/local/bin/mount_virtiofs <<EOF
#!/bin/bash
sudo mkdir -p /workspace
mount -t virtiofs share_fsdev /workspace
EOF
chmod a+x /usr/local/bin/mount_virtiofs

cat > /etc/profile.d/ossim.sh <<EOF
export OSSIM_BUILD=/build
export OSSIM_OUTPUT=/output
export OSSIM_PREFIX=/prefix

export PATH=\$OSSIM_PREFIX/bin\${PATH:+:\$PATH}
export LD_LIBRARY_PATH\${OSSIM_PREFIX}/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}
export LIBRARY_PATH=\${OSSIM_PREFIX}/lib\${LIBRARY_PATH:+:\$LIBRARY_PATH}
export CPATH=\${OSSIM_PREFIX}/include\${CPATH:+:\$CPATH}
export PKG_CONFIG_PATH=\${OSSIM_PREFIX}/lib/pkgconfig:\${OSSIM_PREFIX}/share/pkgconfig\${PKG_CONFIG_PATH:+:\$PKG_CONFIG_PATH}
EOF

. /etc/profile.d/ossim.sh

DIRS="$OSSIM_BUILD $OSSIM_OUTPUT $OSSIM_PREFIX"

mkdir -p $DIRS
chmod a+rwx $DIRS
