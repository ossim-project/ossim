set -xe

INPUT_TAR=${INPUT_TAR:-/dev/sdb}

mkdir -p /tmp/input
pushd /tmp/input

tar xf $INPUT_TAR

cp -r env/ ~
cp -r setup/ ~

popd
rm -rf /tmp/input
