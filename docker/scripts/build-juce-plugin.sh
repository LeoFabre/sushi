#!/usr/bin/env bash
# Cross-compile a JUCE VST3 plugin for PocketBeagle2 (aarch64 / Elk Audio OS).
#
# Usage:
#   build-juce-plugin.sh [PLUGIN_SRC] [BUILD_DIR] [extra cmake args...]
#
# The plugin's CMakeLists.txt must use the standard JUCE CMake API, e.g.:
#
#   cmake_minimum_required(VERSION 3.22)
#   project(MyPlugin)
#
#   find_package(JUCE CONFIG REQUIRED)
#
#   juce_add_plugin(MyPlugin
#       PLUGIN_MANUFACTURER_CODE Mfgr
#       PLUGIN_CODE Plgn
#       FORMATS VST3
#       PRODUCT_NAME "My Plugin"
#       IS_SYNTH FALSE
#       NEEDS_MIDI_INPUT FALSE
#       NEEDS_MIDI_OUTPUT FALSE
#   )
#
#   target_sources(MyPlugin PRIVATE src/PluginProcessor.cpp)
#
#   target_compile_definitions(MyPlugin PUBLIC
#       JUCE_DISPLAY_SPLASH_SCREEN=0
#       JUCE_WEB_BROWSER=0
#       JUCE_USE_CURL=0
#       # Headless: no GUI editor — plugin runs as pure DSP inside Sushi
#       JUCE_HEADLESS_PLUGIN_CLIENT=1
#   )
#
#   target_link_libraries(MyPlugin
#       PRIVATE juce::juce_audio_utils
#       PUBLIC  juce::juce_recommended_config_flags
#               juce::juce_recommended_warning_flags
#   )
#
# VST3_AUTO_MANIFEST is forced OFF: cmake must not execute the ARM64 .so on
# the x86 host to extract moduleinfo.json (would fail with "Exec format error").

set -euo pipefail

PLUGIN_SRC="${1:-/workspace/my-plugin}"
BUILD_DIR="${2:-/workspace/build-plugin}"
shift 2 2>/dev/null || true

# ── Sanity check ──────────────────────────────────────────────────────────────

if [[ ! -f "${PLUGIN_SRC}/CMakeLists.txt" ]]; then
    echo "ERROR: Plugin source not found at ${PLUGIN_SRC}" >&2
    exit 1
fi

# ── Configure ─────────────────────────────────────────────────────────────────

mkdir -p "${BUILD_DIR}"

cmake "${PLUGIN_SRC}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-aarch64-elk.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/opt/juce-installed \
    -DVST3_AUTO_MANIFEST=FALSE \
    -DCMAKE_STRIP=aarch64-linux-gnu-strip \
    "$@"

# ── Build ─────────────────────────────────────────────────────────────────────

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ""
echo "Plugin built successfully."

VST3_BUNDLE=$(find "${BUILD_DIR}" -name "*.vst3" -maxdepth 6 2>/dev/null | head -1)
if [[ -n "${VST3_BUNDLE}" ]]; then
    echo "  VST3 bundle: ${VST3_BUNDLE}"
    echo ""
    echo "  Verify architecture (should show aarch64 ELF):"
    echo "    file \"${VST3_BUNDLE}/Contents/aarch64-linux/\"*.so"
    echo ""
    echo "  Copy to device:"
    echo "    scp -r \"${VST3_BUNDLE}\" root@pocketbeagle2:/usr/lib/vst3/"
fi
