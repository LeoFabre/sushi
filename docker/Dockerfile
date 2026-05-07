# Cross-compilation image for PocketBeagle2 (TI AM625x / Cortex-A53 aarch64)
# running Elk Audio OS with EVL real-time kernel.
#
# What's inside:
#   ▸ aarch64-linux-gnu GCC cross-compiler (from Ubuntu packages)
#   ▸ ARM64 system libraries via multiarch (ALSA, FreeType, libcurl, …)
#   ▸ libevl (EVL real-time userspace) cross-compiled for ARM64
#   ▸ JUCE 8.x installed as a CMake package for headless VST3 cross-builds
#   ▸ vcpkg with an arm64-elk-linux triplet for CMake-findable packages
#     (gRPC, libsndfile, lv2, lilv, …)
#
# ── raspa ──────────────────────────────────────────────────────────────────────
# raspa is a proprietary Elk Audio library not publicly available.
# You need to supply libraspa.so + headers before building Sushi with RASPA ON.
# Two options:
#   1. Copy from your device:
#        scp root@pocketbeagle2:/usr/lib/libraspa.so* raspa/
#        scp -r root@pocketbeagle2:/usr/include/raspa  raspa/include/
#      Then at container run time:
#        docker run -v $(pwd)/raspa/libraspa.so:/opt/elk-sysroot/usr/lib/libraspa.so \
#                   -v $(pwd)/raspa/include:/opt/elk-sysroot/usr/include/raspa \
#                   … elk-crossbuild
#   2. Place libraspa.so and headers in docker/raspa/ and uncomment the
#      COPY lines at the bottom of this file.
#
# ── Build ─────────────────────────────────────────────────────────────────────
#   docker build -t elk-crossbuild docker/
#
# ── Run ───────────────────────────────────────────────────────────────────────
#   docker run --rm -it -v $(pwd):/workspace/sushi elk-crossbuild
#   # inside the container:
#   /opt/scripts/build-sushi.sh /workspace/sushi /workspace/build-sushi
#   /opt/scripts/build-juce-plugin.sh /workspace/my-plugin /workspace/build-plugin

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# ── 1. Host build tools ────────────────────────────────────────────────────────

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        wget \
        curl \
        unzip \
        zip \
        tar \
        pkg-config \
        python3 \
        python3-pip \
        ca-certificates \
        locales \
        # Host-side gRPC tools: protoc + grpc_cpp_plugin run on x86_64 to
        # generate .pb.cc/.grpc.pb.cc files during the cmake configure step;
        # the resulting sources are compiled for ARM64 by the cross-compiler.
        protobuf-compiler \
        libprotobuf-dev \
        libgrpc++-dev \
        protobuf-compiler-grpc \
        # libjack headers are referenced unconditionally in sushi source files
        # (preprocessor guards skip the code, but the include path must exist).
        libjack-jackd2-dev \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

# ── 2. aarch64 cross-compiler ─────────────────────────────────────────────────

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        binutils-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# ── 3. ARM64 system libraries via multiarch ───────────────────────────────────
# Ubuntu multiarch installs arm64 libraries to /usr/lib/aarch64-linux-gnu/
# and arch-specific headers to /usr/include/aarch64-linux-gnu/.
# The CMake toolchain (toolchain-aarch64-elk.cmake) adds these paths to
# CMAKE_FIND_ROOT_PATH so the cross-build can locate them automatically.

RUN dpkg --add-architecture arm64 \
    && apt-get update && apt-get install -y --no-install-recommends \
        # ALSA — for SUSHI_WITH_ALSA_MIDI
        libasound2-dev:arm64 \
        # FreeType — required by JUCE even for headless builds (font metrics)
        libfreetype-dev:arm64 \
        # libcurl — required by JUCE modules (WebInputStream etc.)
        libcurl4-openssl-dev:arm64 \
        libssl-dev:arm64 \
        # liblo — OSC transport (oscpack may use it)
        liblo-dev:arm64 \
        # Standard C/C++ runtime for ARM64
        libc6-dev:arm64 \
        libstdc++-13-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

# ── 4. libevl — EVL real-time userspace library for ARM64 ─────────────────────
# EVL is the modern real-time layer used by Elk Audio OS (replaces Xenomai 3).
# Sushi links against libevl when SUSHI_RASPA_FLAVOR=evl (the default).
#
# The libevl source bundles its own kernel UAPI headers, so no separate
# kernel source tree is required for the userspace build.
#
# Adjust EVL_REF to match the EVL kernel version running on your device.

ARG EVL_GIT="https://git.evlproject.org/libevl.git"
ARG EVL_REF="r36"

RUN git clone --depth 1 --branch "${EVL_REF}" "${EVL_GIT}" /tmp/libevl-src \
    && make -C /tmp/libevl-src \
             ARCH=arm64 \
             CROSS_COMPILE=aarch64-linux-gnu- \
             O=/tmp/libevl-build \
             -j"$(nproc)" \
    && make -C /tmp/libevl-src \
             ARCH=arm64 \
             CROSS_COMPILE=aarch64-linux-gnu- \
             O=/tmp/libevl-build \
             install \
             DESTDIR=/opt/elk-sysroot \
             prefix=/usr \
    && rm -rf /tmp/libevl-src /tmp/libevl-build

# ── 5. raspa placeholder ───────────────────────────────────────────────────────
# raspa is not publicly available. See the header comment for how to provide it.
# The build scripts will warn if libraspa.so is missing.

RUN mkdir -p \
        /opt/elk-sysroot/usr/lib \
        /opt/elk-sysroot/usr/include/raspa

# Uncomment once you've placed raspa files in docker/raspa/:
# COPY raspa/libraspa.so  /opt/elk-sysroot/usr/lib/libraspa.so
# COPY raspa/include/      /opt/elk-sysroot/usr/include/raspa/

# ── 6. JUCE ───────────────────────────────────────────────────────────────────
# JUCE is installed as a CMake package so plugin CMakeLists.txt can use:
#   find_package(JUCE CONFIG REQUIRED)
# JUCE modules are INTERFACE/source targets — they compile with the cross-
# compiler at plugin build time; the install step only copies sources and
# generates CMake config files (architecture-independent).

ARG JUCE_VERSION="8.0.12"

RUN wget -q -O /tmp/juce.zip \
        "https://github.com/juce-framework/JUCE/archive/refs/tags/${JUCE_VERSION}.zip" \
    && unzip -q /tmp/juce.zip -d /opt \
    && mv "/opt/JUCE-${JUCE_VERSION}" /opt/JUCE \
    && rm /tmp/juce.zip \
    && cmake -S /opt/JUCE -B /tmp/juce-build \
             -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_INSTALL_PREFIX=/opt/juce-installed \
             -DJUCE_BUILD_EXTRAS=OFF \
             -DJUCE_BUILD_EXAMPLES=OFF \
    && cmake --install /tmp/juce-build \
    && rm -rf /tmp/juce-build

ENV JUCE_PATH="/opt/JUCE"

# ── 7. vcpkg ──────────────────────────────────────────────────────────────────
# vcpkg cross-compiles CMake-findable packages (gRPC, libsndfile, lv2, lilv)
# for ARM64 using the arm64-elk-linux triplet (copied below).
# The triplet chainloads toolchain-aarch64-elk.cmake which sets the cross-
# compiler and ARM64 sysroot paths.
#
# Pre-install the host (x64-linux) triplet packages that vcpkg uses as build
# tools (protoc, grpc_cpp_plugin) — these are built natively for x86_64.

ENV VCPKG_ROOT="/opt/vcpkg"
ENV VCPKG_DISABLE_METRICS=1

RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" \
    && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics

# ── 8. Toolchain files and build scripts ──────────────────────────────────────

COPY toolchain-aarch64-elk.cmake /opt/toolchain-aarch64-elk.cmake
COPY triplets/arm64-elk-linux.cmake "${VCPKG_ROOT}/triplets/community/arm64-elk-linux.cmake"
COPY scripts/ /opt/scripts/
RUN chmod +x /opt/scripts/*.sh

# ── 9. Environment ────────────────────────────────────────────────────────────

ENV CROSS_COMPILE="aarch64-linux-gnu-"
ENV CC="aarch64-linux-gnu-gcc"
ENV CXX="aarch64-linux-gnu-g++"
ENV AR="aarch64-linux-gnu-ar"
ENV STRIP="aarch64-linux-gnu-strip"
ENV OBJCOPY="aarch64-linux-gnu-objcopy"

# ELK_SYSROOT: overlay for raspa, libevl, and any other libs not in multiarch.
ENV ELK_SYSROOT="/opt/elk-sysroot"

# vcpkg cross-compilation triplets
ENV VCPKG_TARGET_TRIPLET="arm64-elk-linux"
ENV VCPKG_HOST_TRIPLET="x64-linux"

WORKDIR /workspace

SHELL ["/bin/bash", "-c"]
CMD ["/bin/bash"]
