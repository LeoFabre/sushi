# vcpkg triplet — ARM64 Linux cross-compilation for Elk Audio OS
# (PocketBeagle2 / Raspberry Pi 4, aarch64)
#
# Used by Sushi's vcpkg integration to cross-compile CMake-findable packages
# (gRPC, libsndfile, lv2, lilv, …) for the ARM64 target.
#
# Invoke cmake with:
#   -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
#   -DVCPKG_TARGET_TRIPLET=arm64-elk-linux
#   -DVCPKG_HOST_TRIPLET=x64-linux
#
# vcpkg will:
#   1. Build host tools (protoc, grpc_cpp_plugin) using the x64-linux triplet
#      (they run on the build machine during cmake configure / codegen).
#   2. Cross-compile all target libraries using this triplet + the chainloaded
#      toolchain below (aarch64-linux-gnu-gcc/g++ with multiarch sysroot).

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE          dynamic)

# Static libs are safer for embedded deployment — no LD_LIBRARY_PATH gymnastics.
# Override per-port if needed:
#   set(VCPKG_LIBRARY_LINKAGE dynamic)  # for libraspa-ish shared deps
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Chainload our cross-compilation toolchain.  vcpkg sets this before calling
# cmake for each port, so every port inherits the aarch64 compiler, sysroot
# paths, and pkg-config env vars.
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE /opt/toolchain-aarch64-elk.cmake)

# Tell vcpkg where the ARM64 target sysroot libraries are.
# This supplements CMAKE_FIND_ROOT_PATH set in the chainloaded toolchain.
set(VCPKG_SYSROOT "")  # no single sysroot dir; multiarch paths are in the toolchain

# C++ standard — must match what Sushi requires (C++20).
set(VCPKG_CXX_FLAGS "-std=c++20")
set(VCPKG_C_FLAGS   "")
