# CMake cross-compilation toolchain for PocketBeagle2
# Target: TI AM625x / ARM Cortex-A53 (aarch64) running Elk Audio OS + EVL
#
# Usage (direct):
#   cmake /path/to/project -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-aarch64-elk.cmake
#
# Usage (via vcpkg — preferred for Sushi):
#   Declared as VCPKG_CHAINLOAD_TOOLCHAIN_FILE in the arm64-elk-linux triplet;
#   pass -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake.

cmake_minimum_required(VERSION 3.22)

# ── Target system ─────────────────────────────────────────────────────────────

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ── Cross-compiler ────────────────────────────────────────────────────────────

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_AR           aarch64-linux-gnu-ar   CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB       aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP        aarch64-linux-gnu-strip)
set(CMAKE_LINKER       aarch64-linux-gnu-ld)
set(CMAKE_OBJCOPY      aarch64-linux-gnu-objcopy)

# ── CPU tuning ────────────────────────────────────────────────────────────────
# ARMv8-A baseline covers both AM625x (Cortex-A53) and RPi4 (Cortex-A72).
# -mtune=cortex-a53 hints the scheduler without restricting the ISA.
# AArch64 has mandatory NEON/ASIMD — no -mfpu flag needed.

add_compile_options(-march=armv8-a -mtune=cortex-a53)

# ── Sysroot / library search paths ────────────────────────────────────────────
# Ubuntu multiarch installs arm64 libraries to:
#   /usr/lib/aarch64-linux-gnu/          ← shared/static libs
#   /usr/include/aarch64-linux-gnu/      ← arch-specific headers
#   /usr/lib/aarch64-linux-gnu/pkgconfig ← pkg-config files
#
# EVL (libevl) and raspa are installed to:
#   /opt/elk-sysroot/usr/lib/
#   /opt/elk-sysroot/usr/include/

list(APPEND CMAKE_FIND_ROOT_PATH
    /opt/elk-sysroot
    /usr/aarch64-linux-gnu
    /usr/lib/aarch64-linux-gnu
)

# Do not search host paths for target libraries / headers / cmake packages.
# Host tools (cmake generators, protoc, …) are found via PROGRAM mode = NEVER.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── pkg-config ────────────────────────────────────────────────────────────────
# Direct pkg-config to the ARM64 .pc files from multiarch and EVL.
# CMake's FindPkgConfig and vcpkg both honour these env vars.

set(ENV{PKG_CONFIG_PATH}
    "/opt/elk-sysroot/usr/lib/pkgconfig:/usr/lib/aarch64-linux-gnu/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR}
    "/opt/elk-sysroot/usr/lib/pkgconfig:/usr/lib/aarch64-linux-gnu/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/")

# ── JUCE headless / cross-compilation defaults ────────────────────────────────
# These can be overridden per-target in the plugin CMakeLists.txt.
# VST3_AUTO_MANIFEST: must be OFF when cross-compiling — cmake would otherwise
# try to execute the ARM64 plugin binary on the x86 host to extract the
# moduleinfo.json, which fails with "Exec format error".

set(VST3_AUTO_MANIFEST OFF CACHE BOOL "Disable auto VST3 manifest (required for cross-compilation)" FORCE)

# ── Raspberry Pi 4 / PocketBeagle2 compatibility note ─────────────────────────
# Both boards are aarch64 (ARMv8-A). Binaries built with -march=armv8-a run on
# both Cortex-A53 (PocketBeagle2 AM625x) and Cortex-A72 (RPi4 BCM2711).
# If you want PocketBeagle2-only builds for maximum performance, change the
# compile option above to: -march=armv8-a -mcpu=cortex-a53
