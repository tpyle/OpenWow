# syntax=docker/dockerfile:1
#
# packaging/linux/Dockerfile -- build an OpenWoW Linux AppImage from a clean
# checkout using only Docker or Podman: no local CMake, compiler, or vcpkg
# install required.
#
# USAGE (run from the repository root):
#
#   docker build -f packaging/linux/Dockerfile -t openwow-appimage-builder .
#   id=$(docker create openwow-appimage-builder)
#   docker cp "$id":/dist ./dist
#   docker rm "$id" >/dev/null
#
# `podman build` / `podman create` / `podman cp` work identically. The
# AppImage lands in ./dist/OpenWoW-<version>-linux-<arch>.AppImage on the
# host. <arch> follows the build host: running this on an aarch64 host (or
# under aarch64 emulation) produces an aarch64 AppImage, since appimagetool
# and the type-2 runtime are pinned for both x86_64 and aarch64 in
# build-appimage.sh.
#
# This mirrors the linux-x86_64 (gcc) lane of .github/workflows/build.yml:
# same base OS, same apt package list, same vcpkg baseline, the same
# `ci-linux-gcc` CMake preset, and the same packaging/linux/build-appimage.sh
# entry point -- so a locally built AppImage is built the same way a release
# one is. See docs/BUILDING.md for the non-container build and what each step
# below does.
#
# DIAGNOSTIC BUILDS: pass --build-arg OPENWOW_CMAKE_PRESET=ci-linux-gcc-asan
# to build an AddressSanitizer-instrumented client instead (see that
# preset's description in CMakePresets.json) -- useful for reproducing a
# heap-corruption/use-after-free report with an exact stack trace at the
# bad access, rather than wherever glibc's allocator happens to notice the
# damage afterward. The output AppImage is suffixed with the preset name so
# it's never confused with a real release build.
#
# CACHING: the `--mount=type=cache` lines below give vcpkg's built-package
# archives, its downloaded source tarballs, and this project's own object
# files (via ccache) a persistent home outside the image, the same way
# .github/workflows/build.yml keys its "vcpkg-bincache" / "vcpkg/downloads"
# caches on the manifest files rather than on the source tree. A `COPY . .`
# below still invalidates Docker's own layer cache for the configure/build
# RUN steps whenever any source file changes -- that part cannot be avoided
# without hand-maintaining a second, narrower COPY list of every CMakeLists.txt
# in the tree, which would silently go stale as the project grows -- but once
# those RUN steps do re-execute, vcpkg finds its ports already built in the
# archive cache (skipping the ~30-60 minute FFmpeg/Boost/bgfx/SDL2 compile
# entirely when the manifest hasn't changed) and ccache finds the client's own
# unchanged translation units already compiled. A cold build (empty caches)
# still takes 30-60 minutes and several GB of network and disk; this only
# speeds up the *next* build. `--mount=type=cache` requires BuildKit, which is
# the default `docker build` backend on any current Docker install, and is
# natively supported by Podman/Buildah -- no extra flags needed either way.

FROM ubuntu:24.04 AS builder
ARG OPENWOW_CMAKE_PRESET=ci-linux-gcc
ENV DEBIAN_FRONTEND=noninteractive

# Same list as the "Install system packages (Linux)" and "Install packaging
# tools (Linux)" steps in .github/workflows/build.yml (see docs/BUILDING.md
# "Per-OS notes" for what each dependency is for), plus packages the GitHub
# Actions runner image ships preinstalled but a bare ubuntu:24.04 container
# does not: git/curl/ca-certificates/cmake/ninja-build; unzip/tar, which
# vcpkg's own tool bootstrap (fetching cmake/ninja/etc. into
# $VCPKG_ROOT/downloads/tools) shells out to directly and fails without; and
# python3-venv, without which libsystemd's vcpkg port (pulled in transitively
# by sdl2's dbus feature, see vcpkg.json's sdl2 comment) fails its meson
# build at `python3 -m venv` with "ensurepip is not available". ccache and
# lld are this Dockerfile's own additions, unneeded by CI: ccache per
# CACHING above, lld because the ci-linux-gcc-asan preset links with it
# (see that preset's description in CMakePresets.json).
RUN apt-get update -qq && apt-get install -y --no-install-recommends \
      ca-certificates git curl unzip tar python3 python3-venv cmake ninja-build ccache lld \
      build-essential pkg-config \
      autoconf autoconf-archive automake libtool libtool-bin libltdl-dev nasm yasm \
      libx11-dev libxext-dev libxft-dev libxrandr-dev libxi-dev \
      libxcursor-dev libxinerama-dev libxss-dev libxxf86vm-dev \
      libxkbcommon-dev libwayland-dev wayland-protocols \
      libegl1-mesa-dev libgl-dev libglu1-mesa-dev libibus-1.0-dev \
      libfuse2t64 file desktop-file-utils zip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

# 1. vcpkg, pinned to the manifest baseline (docs/BUILDING.md step 1). Fetched
#    by commit instead of a full clone: GitHub serves any reachable commit of
#    a public repo, so this gets the pinned tree without vcpkg's full history.
ENV VCPKG_ROOT=/workspace/.vcpkg
RUN BASELINE="$(python3 -c 'import json;print(json.load(open("vcpkg-configuration.json"))["default-registry"]["baseline"])')" \
    && mkdir -p "$VCPKG_ROOT" && git -C "$VCPKG_ROOT" init -q \
    && git -C "$VCPKG_ROOT" remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C "$VCPKG_ROOT" fetch --depth 1 origin "$BASELINE" \
    && git -C "$VCPKG_ROOT" checkout -q FETCH_HEAD \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

# 2. Configure and build (the ci-linux-gcc preset from CMakePresets.json --
#    the same one build.yml uses). First configure resolves and builds the
#    whole vcpkg manifest; see CACHING above for how subsequent ones speed up.
ENV VCPKG_DEFAULT_BINARY_CACHE=/root/.cache/vcpkg-bincache
# vcpkg's own port builds don't inherit CMAKE_C(XX)_FLAGS from this
# top-level configure (they run through the toolchain file as isolated
# sub-builds with their own flags), so the ASan preset's extra flags don't
# change vcpkg's port ABI hashes: the vcpkg-bincache mount is safely shared
# across ordinary and diagnostic builds. ccache keys on the actual compiler
# invocation, so it's equally safe to share as-is.
RUN --mount=type=cache,target=/root/.cache/vcpkg-bincache,id=openwow-vcpkg-bincache \
    --mount=type=cache,target=/workspace/.vcpkg/downloads,id=openwow-vcpkg-downloads \
    cmake --preset ${OPENWOW_CMAKE_PRESET} \
      -DVCPKG_INSTALL_OPTIONS=--clean-after-build \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
# ASan instruments every memory access, which roughly doubles cc1plus's
# per-translation-unit memory footprint; the largest translation units
# (e.g. apps/client/glue_host/glue_client.cpp, big enough that GCC's
# variable-tracking pass already hits its own internal size limit on a
# plain build) can then push a fully-parallel `ninja` (one job per core)
# past available RAM and get a job OOM-killed -- "g++: fatal error: Killed
# signal terminated program cc1plus", not a real compile error. Capped to 4
# jobs for the ASan preset only; the ordinary build has run unbounded here
# without issue.
RUN --mount=type=cache,target=/root/.ccache,id=openwow-ccache \
    export CCACHE_DIR=/root/.ccache \
    && PARALLEL_ARGS="--parallel" \
    && [ "${OPENWOW_CMAKE_PRESET}" != "ci-linux-gcc-asan" ] || PARALLEL_ARGS="--parallel 4" \
    && cmake --build --preset ${OPENWOW_CMAKE_PRESET} --target openwow-client ${PARALLEL_ARGS} -- -k 0

# 3. Install the client component, then wrap it as an AppImage. Its version
#    comes straight from CMakeLists.txt's project() call so the filename never
#    drifts from what was actually built. build-appimage.sh's own pinned
#    appimagetool/runtime downloads (see its header) get the same cache
#    treatment as vcpkg's, since it re-verifies their sha256 before reusing
#    a cached copy.
ENV OPENWOW_APPIMAGE_CACHE=/root/.cache/openwow-appimage-cache
RUN --mount=type=cache,target=/root/.cache/openwow-appimage-cache,id=openwow-appimage-cache \
    VERSION="$(sed -n -E 's/^project\(OpenWoW VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)" \
    && SUFFIX="" \
    && [ "${OPENWOW_CMAKE_PRESET}" = "ci-linux-gcc" ] || SUFFIX="-${OPENWOW_CMAKE_PRESET}" \
    && rm -rf stage dist && mkdir -p stage dist \
    && cmake --install build/${OPENWOW_CMAKE_PRESET} --component client --prefix stage \
    && bash packaging/linux/build-appimage.sh stage \
         "dist/OpenWoW-${VERSION}-linux-$(uname -m)${SUFFIX}.AppImage"

# Export stage: keeps the image handed back to the host down to just the
# AppImage, not the multi-GB build tree. `docker create` + `docker cp` (see
# the header) copies it out without needing BuildKit's `--output`, so the
# same two commands work with podman.
FROM scratch AS export
COPY --from=builder /workspace/dist/ /
