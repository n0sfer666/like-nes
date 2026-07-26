# Third-party components

like-nes itself is dual-licensed MIT OR Apache-2.0 (see [`LICENSE`](LICENSE)).
The libraries below are linked into the engine and its tools, and remain under
their own licenses. Every entry was verified by reading the license text that
ships with the dependency at the pinned revision, not from memory — see
[Verifying this inventory](#verifying-this-inventory) for where each text lives.

This file is the **inventory**. The verbatim copyright and permission notices
themselves are in [`THIRD-PARTY-NOTICES.txt`](THIRD-PARTY-NOTICES.txt) (C/C++
components), [`THIRD-PARTY-NOTICES-RUST.txt`](THIRD-PARTY-NOTICES-RUST.txt)
(the 138 Rust crates that make up wgpu-native) and
[`THIRD-PARTY-NOTICES-NDK.txt`](THIRD-PARTY-NOTICES-NDK.txt) (the LLVM toolchain
NOTICE that covers `libc++_shared.so`) — those are the files that actually
discharge the notice-retention duty, within the limits recorded under
[Known boundaries](#known-boundaries).

All of them are permissive and compatible with both arms of the project
license. None imposes a royalty, a field-of-use restriction, or a copyleft
obligation on games built with like-nes. Two identifiers in the Rust graph are
worth naming because they are not plain "MIT OR Apache-2.0": `unicode-ident`
carries a **conjunctive** `(MIT OR Apache-2.0) AND Unicode-DFS-2016` — the
Unicode arm is an additional duty, not a choice — and `hexf-parse` is CC0-1.0,
a public-domain dedication with no attribution duty at all.

| Component | Version / pin | License | Where used |
|---|---|---|---|
| [flecs](https://github.com/SanderMertens/flecs) | v4.1.6 | MIT | ECS core (#1), editor reflection (#7) |
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.91.5-docking | MIT | IDE shell (#7), plugin panels (#6) |
| [GLFW](https://github.com/glfw/glfw) | 3.4 | Zlib | desktop windowing / input (#4) |
| [wgpu-native](https://github.com/gfx-rs/wgpu-native) | v0.19.4.1 | MIT **OR** Apache-2.0 | WebGPU backend (#2) |
| wgpu-native's Rust crate graph | 138 crates, pinned by its `Cargo.lock` | permissive mix — full per-crate list in [`THIRD-PARTY-NOTICES-RUST.txt`](THIRD-PARTY-NOTICES-RUST.txt) | statically linked into the iOS/Android binary; inside `libwgpu_native` on desktop |
| [webgpu-headers](https://github.com/webgpu-native/webgpu-headers) | vendored in wgpu-native | BSD-3-Clause | WebGPU C API headers |
| [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) | main-v0.2.0 | MIT | desktop wgpu packaging (#2) |
| [glfw3webgpu](https://github.com/eliemichel/glfw3webgpu) | v1.2.0 | MIT | GLFW↔WebGPU surface glue (#2) |
| [stb](https://github.com/nothings/stb) | pinned SHA | MIT **OR** Unlicense | `stb_image` — image decode in assetc (#5); `stb_vorbis` — audio decode (#3, `poc/audio/stb_vorbis_impl.c`); `stb_image_write` — golden screenshots (`poc/render/capture.cpp`) |
| [Basis Universal](https://github.com/BinomialLLC/basis_universal) | v1.60 | Apache-2.0 | BC7/ETC transcode (#5) — **only `transcoder/` is compiled**; the encoder components, which upstream carries under BSD-3-Clause / MIT / Zlib, are not linked |
| [Zstandard](https://github.com/facebook/zstd) | v1.5.6 | BSD-3-Clause **OR** GPL-2.0 | asset bundle compression (#5) |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.21 | Unlicense **OR** MIT-0 | audio backend (#3) |
| [Wasmtime](https://github.com/bytecodealliance/wasmtime) | v26.0.0 C-API | Apache-2.0 WITH LLVM-exception | WASM plugin sandbox (#6) |
| `android_native_app_glue` (Android NDK) | NDK 28.2.13676358 | Apache-2.0 | Android activity glue, compiled into `libgame.so` |
| LLVM `libc++` (`libc++_shared.so`) | Android NDK 28.2.13676358 | Apache-2.0 WITH LLVM-exception | C++ runtime shipped inside the APK |

## Elections for dual-licensed dependencies

Where a dependency offers a choice, like-nes elects the permissive arm and
distributes under it:

- **Zstandard** — elected **BSD-3-Clause**. The GPL-2.0 arm is *not* used;
  nothing in like-nes or in games built with it becomes GPL by linking zstd.
  This election must be preserved by redistributors who rely on this file.
- **stb** — elected **MIT** (the Unlicense arm is equally acceptable; MIT is
  named for jurisdictions that do not recognise public-domain dedication).
- **miniaudio** — elected **MIT-0**, for the same reason.
- **wgpu-native** — elected **MIT** (its MIT text is the one reproduced in
  `THIRD-PARTY-NOTICES.txt`). The Apache arm remains available upstream if you
  prefer it, but this project distributes under MIT.

## Obligations when you redistribute a binary

Shipping a game built with like-nes triggers only notice-retention duties:

1. **Retain copyright and license notices** for the components above. Shipping
   the three notice files — `THIRD-PARTY-NOTICES.txt`,
   `THIRD-PARTY-NOTICES-RUST.txt` and `THIRD-PARTY-NOTICES-NDK.txt`, which
   reproduce each component's notice verbatim — together with `LICENSE`,
   `LICENSE-MIT`, `LICENSE-APACHE` and this file satisfies this. The engine's
   packaging step (spec #8) installs all seven into the bundle (`licenses/` on
   desktop and in the iOS bundle, `assets/licenses/` in the APK), so a stock
   build ships everything the components below require of it.
2. **Apache-2.0 components** (Basis Universal, Wasmtime, the NDK components, and
   wgpu-native if you take its Apache arm) additionally require that you state
   you have modified the files if you did, and pass along any `NOTICE` file they
   carry. `libc++_shared.so` is shipped in the APK as a whole library rather
   than embedded in compiled output, so the LLVM exception does not relieve that
   duty — the toolchain `NOTICE` travels as `THIRD-PARTY-NOTICES-NDK.txt`.
   like-nes does not modify their sources; it consumes them at pinned
   revisions. Known gap: the Wasmtime C-API SDK ships only a `LICENSE` and no
   `NOTICE`, and its statically linked Rust crate graph is not reproduced here —
   only wgpu-native's is. See *Known boundaries* below.
3. **Wasmtime's LLVM exception** relaxes Apache-2.0's notice requirements for
   compiled output — it is a widening, never a new obligation.

There is nothing to do about the like-nes engine itself beyond keeping its own
notices: no reporting, no registration, no revenue disclosure.

## Known boundaries

- **Wasmtime's crate graph is not enumerated.** `libwasmtime.a` is a prebuilt
  static library that embeds cranelift, wasmparser and the Rust standard
  library; the SDK ships one `LICENSE` (Apache-2.0 WITH LLVM-exception) and no
  per-crate notices, and no `Cargo.lock` is distributed with it. Reproduced here
  is what the SDK actually carries. Wasmtime is used by the plugin sandbox (#6)
  and is not linked into the sample game; enumerating it properly means building
  it from source and running the same generator used for wgpu-native.
- **The Rust standard library is not enumerated.** `libwgpu_native` links
  precompiled `std` / `core` / `alloc`, `compiler-builtins` and the unwinder;
  those are not in wgpu-native's `Cargo.lock`, so they are absent from
  `THIRD-PARTY-NOTICES-RUST.txt`. They are distributed by the Rust project under
  MIT OR Apache-2.0 (see rust-lang/rust `COPYRIGHT`); enumerating them per-crate
  needs a separate pass over the toolchain's own manifest.
- **No per-file SPDX headers.** Only the root texts are in place; `reuse`
  compliance is not claimed.

## Verifying this inventory

Most pins live in `poc/CMakeLists.txt`, `poc/mobile/wgpu_native.cmake` and
`poc/{ios,android}/CMakeLists.txt`; after a configure the corresponding license
text is on disk under `poc/<build-dir>/_deps/<name>-src/`. Five components do
not follow that shape:

- **webgpu-headers** — no separate source tree; the BSD-3-Clause notice is the
  header comment at the top of
  `_deps/webgpu-backend-wgpu-src/include/webgpu/webgpu.h`.
- **glfw3webgpu** — ships no license file; the MIT notice is the header comment
  in `_deps/glfw3webgpu-src/glfw3webgpu.h`.
- **wgpu-native** — the distribution ships only the prebuilt binary
  (`_deps/webgpu-backend-wgpu-src/bin/`) plus the *wrapper's* MIT license; the
  library's own notice comes from `LICENSE.MIT` in the `gfx-rs/wgpu-native`
  repository at the tag recorded in `wgpu-native-git-tag.txt`.
- **Wasmtime** — not fetched by CMake; it is a manually downloaded SDK under
  `poc/deps/wasmtime-<version>-<triple>-c-api/` (gitignored, currently
  `aarch64-macos` only), with its license at `LICENSE` inside that directory.
- **Android NDK components** — `android_native_app_glue.c` and
  `libc++_shared.so` come from the NDK install referenced by
  `poc/android/build_apk.sh`. The notices are
  `sources/android/native_app_glue/NOTICE` (identical to the header comment of
  `android_native_app_glue.c` at the pinned NDK, but the `NOTICE` file is what
  Apache-2.0 section 4(d) attaches to — check that one when the NDK moves) and
  `toolchains/llvm/prebuilt/<host>/NOTICE`, the latter reproduced verbatim as
  `THIRD-PARTY-NOTICES-NDK.txt`.

`THIRD-PARTY-NOTICES-RUST.txt` is generated, not hand-written:
`poc/scripts/gen_rust_notices.py` builds it from the `Cargo.lock` of the pinned
wgpu-native revision (`poc/<build-dir>/_deps/wgpu_native_src-src/Cargo.lock`, or
pass another path as its first argument), reading each crate's own license files
out of `~/.cargo/registry/src/` after `cargo fetch --locked` in that directory.
Re-running it on an unchanged pin must leave the file byte-identical.
Six crates in that lock file are workspace members of wgpu / wgpu-native itself
(`wgpu-core`, `wgpu-hal`, `wgpu-types`, `naga`, `d3d12`, `wgpu-native`) and are
covered by wgpu-native's own MIT OR Apache-2.0 grant.

Re-check this table **and all three notice files** whenever a pin moves — a version
bump can change a license or a copyright year, and a new dependency must be
added before it is merged.
