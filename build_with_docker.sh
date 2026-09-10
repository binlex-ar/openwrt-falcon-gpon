#!/usr/bin/env bash
#
# build_with_docker.sh - Automated local build using Ubuntu Docker container
#
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}/build_out"

mkdir -p "${WORK_DIR}"

echo "=== Starting OpenWrt 25 build for Lantiq Falcon SFP ==="

docker run --rm -it \
  -v "${SCRIPT_DIR}:/src" \
  -v "${WORK_DIR}:/workspace" \
  -w /workspace \
  ubuntu:24.04 bash -c '
    set -e
    apt-get update && apt-get install -y \
      build-essential clang flex bison g++ gawk gcc-multilib g++-multilib \
      gettext git libncurses5-dev libssl-dev python3 rsync unzip zlib1g-dev \
      file wget qemu-utils sudo

    useradd -m builduser && chown -R builduser:builduser /workspace /src
    su - builduser -c "
      set -e
      if [ ! -d openwrt ]; then
        git clone --depth 1 https://github.com/openwrt/openwrt.git openwrt
      fi
      cd openwrt
      cp -r /src/package/kernel/gpon-falcon package/kernel/
      ./scripts/feeds update -a
      ./scripts/feeds install -a
      cp /src/falcon_sfp.diffconfig .config
      make defconfig
      make download -j\$(nproc)
      make -j\$(nproc)
    "
  '

echo "=== Build finished! Firmware is located in ${WORK_DIR}/openwrt/bin/targets/lantiq/falcon/ ==="
