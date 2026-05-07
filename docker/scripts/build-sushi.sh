#!/usr/bin/env bash
# Build Sushi for PocketBeagle2 (aarch64 / Elk Audio OS + EVL).
#
# Usage:
#   build-sushi.sh [SUSHI_SRC] [BUILD_DIR] [extra cmake args...]
#
# Examples:
#   build-sushi.sh /workspace/sushi /workspace/build-sushi
#   build-sushi.sh /workspace/sushi /workspace/build-sushi -DSUSHI_AUDIO_BUFFER_SIZE=128
#
# Prerequisites (inside the container):
#   - git submodules must be initialised in SUSHI_SRC:
#       git submodule update --init --recursive

set -euo pipefail

SUSHI_SRC="${1:-/workspace/sushi}"
BUILD_DIR="${2:-/workspace/build-sushi}"
shift 2 2>/dev/null || true   # remaining args forwarded to cmake

if [[ ! -f "${SUSHI_SRC}/CMakeLists.txt" ]]; then
    echo "ERROR: Sushi source not found at ${SUSHI_SRC}" >&2
    exit 1
fi

mkdir -p "${BUILD_DIR}"

cmake "${SUSHI_SRC}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/opt/toolchain-aarch64-elk.cmake \
    -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET:-arm64-elk-linux}" \
    -DVCPKG_HOST_TRIPLET="${VCPKG_HOST_TRIPLET:-x64-linux}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSUSHI_WITH_RASPA=ON \
    -DSUSHI_RASPA_FLAVOR=evl \
    -DSUSHI_BUILD_TWINE=OFF \
    -DSUSHI_WITH_JACK=OFF \
    -DSUSHI_WITH_PORTAUDIO=OFF \
    -DSUSHI_WITH_ALSA_MIDI=ON \
    -DSUSHI_WITH_RT_MIDI=OFF \
    -DSUSHI_WITH_VST3=ON \
    -DSUSHI_WITH_LV2=ON \
    -DSUSHI_WITH_LV2_MDA_TESTS=OFF \
    -DSUSHI_WITH_RPC_INTERFACE=ON \
    -DSUSHI_AUDIO_BUFFER_SIZE=64 \
    -DSUSHI_WITH_UNIT_TESTS=OFF \
    -DSUSHI_BUILD_WITH_SANITIZERS=OFF \
    "$@"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ""
echo "Sushi built successfully."
echo "  Binary: ${BUILD_DIR}/apps/sushi"
echo ""
echo "Copy to device:"
echo "  scp ${BUILD_DIR}/apps/sushi root@pocketbeagle2:/usr/bin/"
