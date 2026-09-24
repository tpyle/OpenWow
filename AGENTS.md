# AGENTS.md

Orientation for coding agents working in this repository. `CLAUDE.md` at the
repo root is a symlink to this file — Claude Code and any AGENTS.md-aware
tool read the same document.

## What this is

OpenWoW is a from-scratch reimplementation of the World of Warcraft 3.3.5a
(build 12340) client. It is a drop-in binary: you place it inside an
**existing, legitimately-owned** game installation (next to `Data/`,
`Interface/`, `WTF/`) and it plays against era servers (developed against
AzerothCore) using the stock, unmodified FrameXML/GlueXML UI and 3.3.5a
addons. It ships **zero Blizzard assets or code** — none are in this repo,
and none are ever downloaded by it. It is not affiliated with Blizzard
Entertainment.

The renderer is built on bgfx specifically so one render backend targets
Direct3D, Metal, Vulkan and OpenGL; stated target architectures are x86-64,
ARM64 (including Apple Silicon), and RISC-V64. The project is explicitly
described as experimental but functional: login, realm/character select,
world entry, movement, combat, chat, inventory and quests all work.

## Read this before changing behavior

These rules come from `CONTRIBUTING.md` — read it in full before a
behavioral (not build/tooling) change:

- **Correctness means bit-for-bit parity with the original 3.3.5a client's
  observable behavior.** If something written for the original client
  doesn't work against this one, the bug is in this code, not an excuse to
  change the target behavior.
- **Evidence over plausibility.** A patch that makes a symptom disappear
  without explaining *why* the original behaves that way is usually not
  mergeable. State the correct behavior and how you know it.
- **One change per PR, tight scope**, matching the conventions of the
  surrounding code (this matters more than usual here — see the
  architecture notes below on how inconsistent some conventions already
  are; don't invent a third way).
- **Never copy or transcribe text from disassembly or decompilation of the
  original client** into code, comments, commit messages, docs, or issues.
  Describe *observed* behavior (what happens, under what conditions, how it
  differs from this client) instead. This is a hard rule, not a style
  preference — see "About this repository" below for why.

## About this repository (read before trusting `git log`)

Per `README.md`, the canonical form of this project is published as a
**single-commit snapshot with no history**, regenerated wholesale from a
private working tree rather than committed to incrementally. That's because
the private tree's commit messages and comments cite the original binary's
disassembly by symbol and address as research justification — material
that's fine to keep privately but not to redistribute — so each published
snapshot strips that out. The stated consequence: upstream merges PRs by
*applying* them, not by merging a branch.

**This specific local checkout does not match that description** — `git
log` here shows multiple real commits (`OpenWoW: a from-scratch...` through
later "Sync OpenWow through <hash>" commits), i.e. incremental history has
been layered on top of upstream snapshot syncs in this clone/fork. Don't
assume this checkout's commit history is itself meaningful upstream
provenance, and don't assume the README's single-commit description still
holds without checking `git log` yourself.

## License

**AGPL-3.0** (`LICENSE`), with a maintainer-offered commercial dual license
for those who can't meet AGPL terms (see `README.md` "Legal" and
`CONTRIBUTING.md` "Contributor licence terms"). Contributing a PR means
assigning copyright to the maintainer so that dual-licensing can continue to
work — read `CONTRIBUTING.md` before opening one.

Practical effect for dependency choices: because the project itself is
AGPL, copyleft dependencies are *legally* fine here — but every vendored
piece in `third_party/` is in practice more permissive than required (MIT,
public domain, or CC0; see the table below), and the vcpkg-resolved
dependencies (FFmpeg, Boost, SDL2, bgfx, etc.) are fetched at build time,
not vendored or redistributed. Keep that conservative pattern unless you
have a specific reason to diverge.

## Repository map

| Path | What's there |
| --- | --- |
| `src/openwow/` | Almost all implementation code — see "Source architecture" below. |
| `include/openwow/` | A curated *subset* of headers, not a full public API surface — see below. |
| `apps/client/` | The `openwow-client` executable: composition root, SDL2 adapters, scripted scenario runner. |
| `tests/` | Catch2 unit tests (`openwow_tests`) — see "Testing" below. |
| `third_party/` | Vendored dependencies not pulled from vcpkg (StormLib, a modified Lua 5.1, stb, dr_libs, minimp3, the Dear ImGui SDL2 backend). |
| `cmake/` | Build-option definitions, the shared `openwow_configure_target`/warning-flag setup, ThinLTO/PGO wiring, shader compilation, CPack packaging. |
| `packaging/` | Per-OS packaging inputs: Linux `.desktop`/AppRun/AppImage builder script/Dockerfile, macOS `Info.plist`/notarization script, Windows manifest/resource templates/signing script, shared icons. |
| `vcpkg-overlay-ports/`, `vcpkg-overlay-triplets/` | Patches/overrides for `sdl2` and `bgfx` vcpkg ports, and osx triplet tweaks — see "Build system" below. |
| `docs/` | `BUILDING.md` — the full build guide (per-OS deps, presets, container build, packaging). Currently the only file here. |
| `.github/workflows/` | `ci.yml` (every push/PR, unpackaged build), `release.yml` (on `v*` tags, packaged build + GitHub Release), `build.yml` (the shared reusable matrix both call — read this one for the ground-truth build steps). |
| `CONTRIBUTING.md`, `README.md`, `THIRD_PARTY_NOTICES.md`, `LICENSE` | Project policy, overview, and licensing — read these, don't infer them from code. |

## Source architecture

### `include/openwow/` vs `src/openwow/` — not a public/private split

It looks like a conventional public-headers-vs-implementation split but
isn't quite. `include/openwow/` holds two kinds of things:

1. Small, dependency-free primitives with no matching `.cpp` (most of
   `foundation/math/*.h` — dozens of single-purpose header-only functions).
2. Cross-module **port/contract headers** — abstract interfaces that a
   different module or `apps/client` implements (e.g. `ui/glue/GlueHost`,
   discussed below, lives under `src/openwow/ui/glue/`, but other
   contracts are promoted to `include/`).

Everything else — the large majority of the codebase, including all of
`src/openwow/game/` and `src/openwow/ui/` — keeps headers colocated with
their `.cpp` files in `src/` and is never promoted to `include/`.

Critically, **`include/` is not added to every target's include path**.
`src/openwow/CMakeLists.txt` adds `${PROJECT_SOURCE_DIR}/src` as a public
include root for every component, so `#include "openwow/foo/bar.h"` can
resolve to a header under `src/openwow/foo/` **or** under
`include/openwow/foo/` depending on which target you're building and
whether its `CMakeLists.txt` opted into `${PROJECT_SOURCE_DIR}/include`
(only some leaf `CMakeLists.txt` files do, e.g. `foundation`'s). If you add
a header, check where sibling headers in that module already live and
match it — don't assume `include/` vs `src/` placement from the path alone.

### Module responsibilities (`src/openwow/*`)

| Module | Responsibility |
| --- | --- |
| `foundation` | Math/hashing/diagnostics/memory/text primitives. Mostly promoted to `include/`. |
| `core` | Client init, cvars, FPU control — central runtime glue. |
| `runtime` | Bootstrap and scheduling (frame scheduler, event scheduler). |
| `data` | MPQ-era file format parsing: DBC, BLP, M2 paths, resource validation. |
| `vfs` | Virtual filesystem layered over StormLib/MPQ. |
| `net` | The actual socket/connection I/O and packet dispatch. |
| `network` | Wire-format definitions only (opcodes, `ByteBuffer`, packed GUIDs) — a data-format module, distinct from `net/`'s I/O. |
| `auth` | SRP6 login-server handshake. |
| `audio` | Sound engine. |
| `media` | Video/movie playback (FFmpeg-backed). |
| `render` | bgfx-backed renderer; `render/backend/bgfx` is its own CMake subdirectory. |
| `ui` | The Lua 5.1 FrameXML/GlueXML runtime, widgets, and the glue ports `apps/client` implements. |
| `game` | All gameplay domains — by far the largest module. See the layering note below before assuming its internal structure is uniform. |
| `world`, `input`, `platform`, `storage`, `debug` | As named. |
| `era` | Small (`era_profile.h`, `era_registry.cpp`); reads like a hook for gating behavior by content/client version rather than something currently load-bearing — verify before relying on it. |
| `screens` | Just loading-screen state; small and standalone. |

### `game/` domain layering — a loose convention, not a rule

Many domains under `src/openwow/game/` are organized into `adapters/`
(protocol/lua/ui/platform integration), `application/` (use-case
orchestration) and `rules/`/`model/` (pure domain logic) subfolders — e.g.
`achievements/` has all four. **Do not assume this is uniform.** Checking
several domains directly:

- `combat/` has `adapters/`, `application/`, plus domain-specific `death/`
  and `logs/` — no `rules/` or `model/`.
- `spells/` has `adapters/` plus domain-specific `glyphs/`, `spellbook/`,
  `talents/` — no `application/`/`model/`/`rules/` at all.
- `inventory/` has `adapters/` plus `equipment/`, `items/`, `loot/`,
  `operations/`, `search/`.
- `quests/` has only `adapters/`.

`adapters/` is the one folder that's genuinely close to universal (isolating
integration code from domain logic); the rest of each domain's internal
layout is domain-specific and was not applied as a repo-wide mandate.

More importantly, **this folder structure mostly does not correspond to
separate CMake targets.** `src/openwow/game/CMakeLists.txt` builds one
large `openwow_game` static library containing 600+ `.cpp` files spanning
nearly every domain (combat, spells, inventory, movement, quests, and most
others are all just source-list entries in this one target — the
`adapters/application/rules` folders under them are organizational only).
Only a handful of domains have actually been factored into their own
fine-grained CMake targets with real target-level `adapters`/`application`/
`rules` separation — currently `achievements`, `actions` (incl.
`actions/macros`), `activities/dance`, `calendar`, and `quests` (`dance` is
a sibling of `calendar` under `game/activities/`, not nested inside it),
each added via its own `add_subdirectory(game/...)` call in
`src/openwow/CMakeLists.txt` and linked into `openwow_game` as a private
dependency. If you're adding a new domain or splitting an existing one out,
look at `game/achievements/CMakeLists.txt` as the template, not at the
monolithic `game/CMakeLists.txt` source list.

### Target boilerplate: two generations

`src/openwow/CMakeLists.txt` defines `openwow_add_component(target
sources...)` — a helper that creates a `STATIC` library, wires the standard
include directories (`src/` public, project root private, `third_party/`
as a system include), and calls `openwow_configure_target` (from
`cmake/ProjectOptions.cmake`: sets C++20, warning flags, optional
`-Werror`/clang-tidy). Most `add_subdirectory` calls after that function is
defined use it.

A handful of subdirectories are added *before* `openwow_add_component` is
defined in that same file (`foundation`, `data/formats`,
`network/serialization`, `network/protocol`, `ui/display/settings`,
`ui/runtime/lua`, `ui/widgets/media`) and so cannot use it — they call
`add_library` and `target_include_directories` directly, then call
`openwow_configure_target` themselves. Don't be surprised that these don't
match the newer boilerplate; it's ordering, not inconsistency to fix.

### `apps/client/` — ports and adapters

- **`composition/`** — the executable's composition root: `main.cpp` and
  helpers that resolve the game data directory, wire up the window/render
  backend, and construct everything else. Start here to trace how the
  binary boots.
- **`glue_host/`** — concrete SDL2 adapters for ports the library defines
  abstractly. Confirmed example: `openwow::ui::glue::GlueHost` is an
  abstract interface declared in `src/openwow/ui/glue/glue_host.h`, and
  `SdlGlueHost` in `apps/client/glue_host/sdl_glue_host.h` is its only real
  implementation, backed by SDL2/audio/vfs. If you need to understand what
  the UI layer expects from its host environment, read the port, not the
  adapter.
- **`scenarios/`** — a scripted driver (`ScenarioRunner`,
  `offline_world_fixture`, `scenario_world_oracle`/`ui_driver`) that can
  inject UI actions and movement commands against the live client or an
  offline world fixture. This is the project's de facto integration-test
  and manual-QA harness — it is **not** a Catch2 suite (see Testing below).

## Testing

`tests/` holds a Catch2 unit-test suite (`openwow_tests`, registered with
CTest via `catch_discover_tests`). It is added when `BUILD_TESTING` is on:
the `ci-*` presets leave it at CTest's default (ON), while the local
`release`/`debug` presets and `ci-windows-arm64` (a cross build that can't
run tests) set it OFF, so pass `-DBUILD_TESTING=ON` locally. CI's
"Build and run unit tests" step builds and runs it on every CI lane.

```sh
cmake --preset ci-linux-gcc
cmake --build --preset ci-linux-gcc --target openwow_tests
ctest --test-dir build/ci-linux-gcc --output-on-failure
```

Coverage is concentrated on dependency-light code: `foundation` (math,
hashing, text, containers, memory, threading), `network/serialization`,
`data` format parsers, the server patch-file-name check, and the frame
scheduler. To keep the test binary from linking the large runtime
libraries, a few single-file units with no heavy dependencies are compiled
straight into it (see the comment in `tests/CMakeLists.txt`); follow that
pattern for new pure-logic tests. Most gameplay, UI and rendering code has
no unit tests yet; `apps/client/scenarios/` (above) remains the integration
harness. The filenames `terrain_aabb_test.{h,cpp}` (`data`) and
`sfile_test_support.{h,cpp}` (`vfs/retail`) are runtime helpers, not tests.

## Build system

Dependencies come from a vcpkg manifest (`vcpkg.json`) pinned to a baseline
commit in `vcpkg-configuration.json`, with local overlay ports for `sdl2`
(adds the `alsa` feature Linux needs, plus small patches) and `bgfx` (a
stack of upstream-unmerged bug fixes — Metal/Vulkan/NVTT/shaderc — layered
onto a pinned release). `CMakePresets.json` defines the build presets;
`ci-linux-gcc`/`ci-linux-clang`/`ci-macos-*`/`ci-windows-*` are what CI
actually uses, `release`/`debug`/`release-lto`/`release-pgo-*` are for
local development.

Full instructions, per-OS system packages, and packaging: **`docs/BUILDING.md`**.
Short version:

```sh
cmake --preset release
cmake --build build/release --target openwow-client
```

For a Linux AppImage with no local toolchain at all, see
`packaging/linux/Dockerfile` (needs only Docker or Podman) — its header
comment and `docs/BUILDING.md`'s "Container build" section explain the
caching strategy and usage.

## CI and packaging

`.github/workflows/build.yml` is the single source of truth for how each
platform actually builds and packages — read it directly rather than
inferring build steps from `CMakeLists.txt` alone, since it also captures
system-package lists and known gotchas (see its inline comments, e.g. why
`libltdl-dev`/`autoconf-archive` are needed for an indirect dbus
dependency). `ci.yml` calls it unpackaged on every push/PR; `release.yml`
calls it packaged on `v*` tags and then publishes a GitHub Release with
`SHA256SUMS` once every lane succeeds. `cmake/Packaging.cmake` drives CPack
for the `client` install component (AppImage on Linux via
`packaging/linux/build-appimage.sh`, a signed/notarized `.app` zip on
macOS, a signed `.exe` zip on Windows) — all three optional signing paths
degrade gracefully to unsigned output when the corresponding secret isn't
set.

## Third-party licensing (`third_party/`)

| Component | License |
| --- | --- |
| StormLib (vendored source, not a submodule) | MIT (bundles libtommath/zlib/bzip2/LZMA-SDK under their own permissive terms) |
| wow_lua (modified Lua 5.1) | MIT |
| Dear ImGui SDL2 backend (backend only, not ImGui itself) | MIT |
| stb | Public domain / MIT |
| dr_libs | Public domain / MIT-0 |
| minimp3 | CC0 |

vcpkg-resolved dependencies (FFmpeg, Boost, SDL2, bgfx, FreeType, OpenSSL,
etc.) are fetched at build time under their own licenses and are not
tracked in `THIRD_PARTY_NOTICES.md`, since nothing from them is vendored or
redistributed by this repo.

## Dev tooling

- `.clang-format` — LLVM base style, 2-space indent, 100-column limit.
- `.clang-tidy` — `bugprone-*`, `clang-analyzer-*`, `modernize-*`,
  `performance-*` checks (minus trailing-return-type modernization), scoped
  to `src/|apps/|tests/`. Off by default; enable via
  `-DOPENWOW_ENABLE_CLANG_TIDY=ON`.
- `.clangd` — strict unused/missing-include diagnostics for editor tooling.
- `.editorconfig` — UTF-8, LF, 2-space indent everywhere, trailing
  whitespace trimmed (Markdown excepted).
