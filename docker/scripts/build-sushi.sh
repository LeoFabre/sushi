#!/usr/bin/env bash
# Build Sushi for PocketBeagle2 (aarch64).
#
# Usage:
#   build-sushi.sh [--reactive] [SUSHI_SRC] [BUILD_DIR] [extra cmake args...]
#
# Modes:
#   (default)   Standalone Sushi for Elk Audio OS.
#               Uses raspa + EVL for hard-RT audio. Requires Elk Audio OS on device.
#
#   --reactive  Sushi as a library for embedding inside a Bela render() callback.
#               Bela's PRU + Xenomai RT thread drives the audio — no raspa/EVL needed.
#               Better latency than standalone mode on Bela hardware.
#
# Examples:
#   build-sushi.sh /workspace/sushi /workspace/build-elk
#   build-sushi.sh --reactive /workspace/sushi /workspace/build-bela
#   build-sushi.sh --reactive /workspace/sushi /workspace/build-bela -DSUSHI_AUDIO_BUFFER_SIZE=32
#
# Prerequisites:
#   git submodule update --init --recursive   (in SUSHI_SRC)

set -euo pipefail

# ── Parse --reactive flag ──────────────────────────────────────────────────────

REACTIVE=false
if [[ "${1:-}" == "--reactive" ]]; then
    REACTIVE=true
    shift
fi

SUSHI_SRC="${1:-/workspace/sushi}"
BUILD_DIR="${2:-/workspace/build-sushi}"
shift 2 2>/dev/null || true   # remaining args forwarded to cmake

# ── Sanity check ──────────────────────────────────────────────────────────────

if [[ ! -f "${SUSHI_SRC}/CMakeLists.txt" ]]; then
    echo "ERROR: Sushi source not found at ${SUSHI_SRC}" >&2
    exit 1
fi

# ── Twine detection ───────────────────────────────────────────────────────────
# twine is Elk Audio's RT-aware threading library (private repo).
# It enables multi-core audio processing across the 4 Cortex-A53 cores.
# Initialise the submodule if you have access to elk-audio/twine:
#   git submodule update --init twine

TWINE_AVAILABLE=false
if [[ -f "${SUSHI_SRC}/twine/CMakeLists.txt" ]]; then
    TWINE_AVAILABLE=true
fi

# ── Common cmake flags ────────────────────────────────────────────────────────

COMMON_FLAGS=(
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=/opt/toolchain-aarch64-elk.cmake
    -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET:-arm64-elk-linux}"
    -DVCPKG_HOST_TRIPLET="${VCPKG_HOST_TRIPLET:-x64-linux}"
    -DCMAKE_BUILD_TYPE=Release
    -DSUSHI_WITH_JACK=OFF
    -DSUSHI_WITH_PORTAUDIO=OFF
    -DSUSHI_WITH_ALSA_MIDI=ON
    -DSUSHI_WITH_RT_MIDI=OFF
    -DSUSHI_WITH_VST3=ON
    -DSUSHI_WITH_LV2=ON
    -DSUSHI_WITH_LV2_MDA_TESTS=OFF
    -DSUSHI_WITH_RPC_INTERFACE=ON
    -DSUSHI_AUDIO_BUFFER_SIZE=64
    -DSUSHI_WITH_UNIT_TESTS=OFF
    -DSUSHI_BUILD_WITH_SANITIZERS=OFF
)

# ── Mode-specific flags ───────────────────────────────────────────────────────

mkdir -p "${BUILD_DIR}"

if $REACTIVE; then
    # ── Reactive / Bela library mode ──────────────────────────────────────────
    # Sushi processes audio when your Bela render() calls sushi->process().
    # No raspa or EVL: Bela's PRU + Xenomai thread is the RT driver.
    #
    # Twine note: if enabled, twine creates worker threads for multi-core DSP.
    # On Bela + Xenomai, those threads should ideally be Xenomai threads too.
    # Build twine with TWINE_WITH_XENOMAI=ON to ensure that, otherwise they
    # are plain pthreads (safe but may trigger Xenomai mode-switches).

    echo "Mode: reactive library — Bela render() drives audio, no raspa/EVL"

    if $TWINE_AVAILABLE; then
        echo "  Twine: found — multi-core DSP across 4 Cortex-A53 cores"
        TWINE_FLAGS=(-DSUSHI_BUILD_TWINE=ON -DTWINE_WITH_XENOMAI=ON -DTWINE_WITH_TESTS=OFF)
    else
        echo "  Twine: not found — single-core mode (init twine submodule for multi-core)"
        TWINE_FLAGS=(-DSUSHI_BUILD_TWINE=OFF)
    fi

    cmake "${SUSHI_SRC}" -B "${BUILD_DIR}" \
        "${COMMON_FLAGS[@]}" \
        "${TWINE_FLAGS[@]}" \
        -DSUSHI_WITH_RASPA=OFF \
        -DSUSHI_BUILD_STANDALONE_APP=ON \
        "$@"

else
    # ── Standalone / Elk Audio OS mode ────────────────────────────────────────
    # Sushi runs as a standalone process on Elk Audio OS.
    # raspa + EVL provide the hard-RT audio callback.
    # twine is expected from the system sysroot (SUSHI_BUILD_TWINE=OFF).

    echo "Mode: standalone Elk Audio OS — raspa/EVL RT frontend"

    cmake "${SUSHI_SRC}" -B "${BUILD_DIR}" \
        "${COMMON_FLAGS[@]}" \
        -DSUSHI_WITH_RASPA=ON \
        -DSUSHI_RASPA_FLAVOR=evl \
        -DSUSHI_BUILD_TWINE=OFF \
        "$@"
fi

# ── Build ─────────────────────────────────────────────────────────────────────

cmake --build "${BUILD_DIR}" -j"$(nproc)"

# ── Report ────────────────────────────────────────────────────────────────────

echo ""

if $REACTIVE; then
    LIB="${BUILD_DIR}/libsushi_library.a"
    echo "Sushi reactive library built."
    echo "  Library : ${LIB}"
    echo "  Headers : ${SUSHI_SRC}/include/sushi/"
    echo ""
    echo "In your Bela project CMakeLists.txt:"
    echo "  add_library(sushi_library STATIC IMPORTED)"
    echo "  set_target_properties(sushi_library PROPERTIES"
    echo "      IMPORTED_LOCATION ${LIB})"
    echo "  target_include_directories(my_project PRIVATE ${SUSHI_SRC}/include)"
    echo "  target_link_libraries(my_project sushi_library)"
    echo ""
    echo "In render.cpp:"
    echo "  #include <sushi/standalone_factory.h>  // or reactive_factory.h"
    echo "  // see ${SUSHI_SRC}/include/sushi/reactive_factory.h"
else
    echo "Sushi standalone built."
    echo "  Binary: ${BUILD_DIR}/apps/sushi"
    echo ""
    echo "Copy to device:"
    echo "  scp ${BUILD_DIR}/apps/sushi root@pocketbeagle2:/usr/bin/"
fi
