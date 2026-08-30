# Building OpenWoW

The client builds with CMake and Ninja on macOS, Linux and Windows.
Dependencies come from a **vcpkg manifest** (`vcpkg.json`, baseline pinned in
`vcpkg-configuration.json`, overlay ports in `vcpkg-overlay-ports/`), so you do
not install them by hand.

You need a C++20 toolchain and CMake 3.24 or newer.

## 0. Container build (Docker/Podman, no local tooling)

`packaging/linux/Dockerfile` builds a Linux AppImage end to end -- system
packages, vcpkg, configure, build, and `packaging/linux/build-appimage.sh` --
inside a container, so the only thing you need on the host is Docker or
Podman:

```sh
docker build -f packaging/linux/Dockerfile -t openwow-appimage-builder .
id=$(docker create openwow-appimage-builder)
docker cp "$id":/dist ./dist
docker rm "$id" >/dev/null
```

(`podman` in place of `docker` works the same way.) See the Dockerfile's
header comment for what each step does and how long a cold build takes. The
rest of this document covers building directly on the host, which the
Dockerfile mirrors step for step.

## 1. vcpkg bootstrap

The presets expect a vcpkg checkout at `.vcpkg` (git-ignored) at the repository
root. Use the baseline commit the manifest is pinned to, so you get the same
dependency versions the project was built against:

```sh
BASELINE=$(python3 -c 'import json;print(json.load(open("vcpkg-configuration.json"))["default-registry"]["baseline"])')
git clone https://github.com/microsoft/vcpkg .vcpkg
git -C .vcpkg checkout "$BASELINE"
./.vcpkg/bootstrap-vcpkg.sh -disableMetrics      # bootstrap-vcpkg.bat on Windows
```

The first configure builds every dependency (FFmpeg, Boost, bgfx, SDL2, ...) —
**expect 30–60 minutes cold**. Enable vcpkg binary caching
(`VCPKG_BINARY_SOURCES`) if you build on more than one machine.

## 2. Build

```sh
cmake --preset release
cmake --build --preset release --target openwow-client -j 4
```

On macOS the build target is a Mach-O executable; the install rules assemble
the runnable application bundle. After the compile step, create the normal
development/validation bundle with:

```sh
cmake --install build/release --component client --prefix build/release/bundle
# the app is build/release/bundle/OpenWoW.app
```

The bundle continues to resolve the configured local content root or an
explicit `--game-data` / `OPENWOW_GAME_DATA` override. Use the bare
`build/release/apps/client/openwow-client` only for intermediate compile and
link checks; hand off `OpenWoW.app` for macOS runtime validation. The install
step applies an ad-hoc signature to the completed bundle so its executable and
resources form one valid local-development application. Distribution still
requires the project's intended Developer ID signing and notarization policy.

Available configure presets (Ninja everywhere, build tree `build/<preset>`):

| Preset | Purpose |
| --- | --- |
| `debug` | Debug build |
| `release` | Optimised build |
| `release-lto` | Release + ThinLTO (`OPENWOW_ENABLE_THINLTO=ON`) |
| `release-pgo-generate` | Release, instrumented for profile generation |
| `release-pgo-use` | Release + ThinLTO using the generated profile |

Build presets of the same names exist for each. The PGO lanes are used in
sequence: build with `release-pgo-generate`, exercise the client, then build
`release-pgo-use`, which reads the profile from
`build/release-pgo-generate/pgo/openwow.profdata`.

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `OPENWOW_BUILD_CLIENT` | ON | build `openwow-client` |
| `OPENWOW_ENABLE_MPQ_VFS` | ON | StormLib-backed MPQ VFS (required to read a game install) |
| `OPENWOW_WARNINGS_AS_ERRORS` | OFF | `-Werror` / `/WX` |
| `OPENWOW_ENABLE_CLANG_TIDY` | OFF | run clang-tidy while compiling |
| `OPENWOW_ENABLE_THINLTO` | OFF | ThinLTO (clang) / LTCG (MSVC) / `-flto=auto` (GCC) |

## 3. Per-OS notes

**macOS** — Xcode command line tools, plus
`brew install cmake ninja nasm autoconf automake libtool` (nasm and the
autotools are needed by the FFmpeg port). Set `CMAKE_OSX_DEPLOYMENT_TARGET` (or
`MACOSX_DEPLOYMENT_TARGET` in the environment) if the binary must run on an
older macOS than the build host; `packaging/macos/Info.plist.in`'s
`LSMinimumSystemVersion` mirrors whatever the executable was linked for.

**Linux (Debian/Ubuntu)** — what the vcpkg ports build against:

```
build-essential ninja-build pkg-config autoconf autoconf-archive automake libtool libtool-bin libltdl-dev nasm yasm python3
libx11-dev libxext-dev libxft-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
libxss-dev libxxf86vm-dev libxkbcommon-dev libwayland-dev wayland-protocols
libegl1-mesa-dev libgl-dev libglu1-mesa-dev libibus-1.0-dev
```

plus `libfuse2t64 file desktop-file-utils zip` for the AppImage step. On aarch64
hosts vcpkg needs `VCPKG_FORCE_SYSTEM_BINARIES=1`.

**Windows** — Visual Studio 2022 with the C++ workload. Configure from a
Developer Command Prompt (`vcvarsall x64`) so Ninja finds `cl.exe`.
`SDL2::SDL2main` is linked automatically and the executable is a GUI-subsystem
app, so it opens without a console window.

## 4. Running

The client needs the data files from your own copy of the game — the `Data/`
directory containing the MPQ archives. None of that is distributed here, and
the client will not start without it.

## 5. Packaging

`cmake/Packaging.cmake` drives CPack for the `client` install component: ZIP on
macOS and Windows, TGZ on Linux. The `packaging/` directory holds the platform
scaffolding — the macOS `Info.plist` template, the Windows manifest and
resource templates, and the Linux AppImage runner and desktop entry.

```sh
cmake --build build/release --target package
```
