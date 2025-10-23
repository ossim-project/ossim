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
EOF

. /etc/profile.d/ossim.sh

DIRS="$OSSIM_BUILD $OSSIM_OUTPUT $OSSIM_PREFIX"

mkdir -p $DIRS
chmod a+rwx $DIRS
