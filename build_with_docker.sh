#!/usr/bin/env bash
#
# build_with_docker.sh - Automated local build using Ubuntu Docker container on Mac
#
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="${SCRIPT_DIR}/build_out"

mkdir -p "${WORK_DIR}"

echo "=== Starting OpenWrt 25 build for Lantiq Falcon SFP with Docker ==="
echo "Source: ${SCRIPT_DIR}"
echo "Output: ${WORK_DIR}"

# Use named volume 'openwrt_falcon_build' for fast, native case-sensitive ext4 filesystem in Docker
docker run --rm \
  -v "${SCRIPT_DIR}:/src:ro" \
  -v "${WORK_DIR}:/output" \
  -v openwrt_falcon_build:/workspace \
  -w /workspace \
  ubuntu:24.04 bash -c "
    set -e
    export DEBIAN_FRONTEND=noninteractive
    apt-get update && apt-get install -y \
      build-essential clang flex bison g++ gawk \
      gettext git libncurses5-dev libssl-dev python3 python3-setuptools python3-pip \
      rsync unzip zlib1g-dev file wget qemu-utils sudo device-tree-compiler automake autoconf libtool

    id -u builduser &>/dev/null || useradd -m -s /bin/bash builduser
    chown -R builduser:builduser /workspace

    su - builduser -c '
      set -e
      if [ ! -d /workspace/openwrt ]; then
        echo \"=== Cloning OpenWrt repository ===\"
        git clone --depth 1 https://github.com/openwrt/openwrt.git /workspace/openwrt
      fi
      cd /workspace/openwrt

      echo \"=== Injecting GPON drivers, services, and DTS patches ===\"
      mkdir -p package/kernel package/network/services
      cp -rf /src/package/kernel/gpon-falcon package/kernel/
      cp -rf /src/package/network/services/* package/network/services/
      if [ -d /src/target ]; then
        cp -rf /src/target/* target/
      fi

      ./scripts/feeds update -a
      ./scripts/feeds install -a

      echo \"=== Configuring OpenWrt target ===\"
      cp /src/falcon_sfp.diffconfig .config
      make defconfig

      echo \"=== Downloading sources ===\"
      make download -j\$(nproc)

      make package/network/services/omcid/clean || true

      echo \"=== Compiling toolchain, kernel and images (using \$(nproc) cores) ===\"
      make -j\$(nproc) || make -j1 V=s
    '

    echo \"=== Copying built firmware to host output directory ===\"
    mkdir -p /output
    cp -rf /workspace/openwrt/bin/targets/lantiq/falcon/* /output/
    chmod -R 777 /output
  "

echo "=== Local build finished! Firmware is located in ${WORK_DIR} ==="
