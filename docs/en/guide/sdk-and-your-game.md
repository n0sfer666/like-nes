# The SDK and your own game

English · [Русский](../../ru/guide/sdk-and-your-game.md)

A game does not live inside the engine's tree. It is a CMake project of its own that finds an
**installed SDK** with `find_package`, links two targets and bakes its content into one
`game.bundle` at build time. This page goes from an empty prefix to a running game, and uses
`games/neon-rumble/` throughout. That is the real game, which CI builds against the installed SDK
on all three operating systems, so the files shown below are its own files, not copies.

## Install the SDK into a prefix

The SDK is the `sdk` install component: public headers, the static engine libraries in Release
**and** Debug, the wgpu runtime, `library.bundle` with the materials, the CMake package and the
`assetc` baker. Both configurations go into **one** prefix. Run this from the root of the engine's
clone, after the [build](../getting-started/build.md) prerequisites:

```sh
cmake -S . -B build-sdk-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF
cmake --build build-sdk-release --target like_nes_sdk assetc
cmake --install build-sdk-release --config Release --component sdk --prefix ../like-nes-sdk
cmake -S . -B build-sdk-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF -DIDE_POC=OFF
cmake --build build-sdk-debug --target like_nes_sdk
cmake --install build-sdk-debug --config Debug --component sdk --prefix ../like-nes-sdk
```

On Windows these commands run from the `scripts\win-dev.bat shell` window, where `cl.exe` is
available. The headers a game may include are listed one by one in `cmake/install_sdk.cmake`. An
engine header missing from that list is internal, and a game that includes it does not compile
against the prefix. That is on purpose.

`bash scripts/check_sdk_game.sh --keep` runs the same six steps, builds Neon Rumble against the
result and keeps everything in `build-sdk-work/`. It is the shortest way to a prefix you can trust.

### A mobile prefix

A phone build needs its own prefix per platform: `ios-sim`, `ios-device` or `android-<abi>`. It is
built in Release only and has no `assetc`, because the baker runs on the build machine: the game
passes the desktop prefix as `LIKE_NES_HOST_PREFIX`, and its `assetc` bakes the bundles. The
paths below must be absolute, because a cross toolchain does not resolve a relative
`like-nes_DIR`. For the iOS simulator:

```sh
cmake -S . -B build-sdk-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-sdk-ios-sim --target like_nes_sdk
cmake --install build-sdk-ios-sim --component sdk --prefix <ios prefix>
cmake -S games/neon-rumble -B build-game-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -Dlike-nes_DIR=<ios prefix>/lib/cmake/like-nes -DLIKE_NES_HOST_PREFIX=<desktop prefix>
cmake --build build-game-ios-sim
```

For Android the SDK and the game are configured with the toolchain file of NDK 28.2.13676358,
`-DCMAKE_TOOLCHAIN_FILE=<NDK>/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64
-DANDROID_PLATFORM=android-29`, in place of the iOS flags. The game build then also packages
`neon_rumble.apk`. The prefix remembers what it was built for: a game with another minimum iOS,
another API level or another NDK revision is refused by `find_package` with the value to use.
`check_sdk_game.sh` builds both prefixes after the desktop half (iOS on macOS, Android on macOS and
Linux with the pinned NDK). It checks that each package carries the binary of its platform, both
bundles and the licenses, and that its `game.bundle` is byte for byte the desktop one.

## The game's CMakeLists.txt

This is the whole build file of Neon Rumble:

<!-- snippet: games/neon-rumble/CMakeLists.txt -->
```cmake
cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)

set(RUMBLE_SOURCES src/rumble.cpp src/rumble_level.cpp src/rumble_layers.cpp
  src/rumble_fighter.cpp src/rumble_fighter_quads.cpp src/rumble_credits.cpp src/rumble_brawl.cpp
  src/rumble_roster_quads.cpp src/rumble_report.cpp src/rumble_brawl_report.cpp src/rumble_kinds.cpp
  src/rumble_depth_overlay.cpp src/rumble_controls.cpp src/rumble_hotseat.cpp src/rumble_banner.cpp
  src/rumble_gpu_frame.cpp src/rumble_wave.cpp src/rumble_wave_read.cpp src/rumble_facing.cpp
  src/rumble_floor.cpp src/rumble_brain.cpp src/rumble_wave_script.cpp src/rumble_steer.cpp
  src/rumble_menu.cpp src/rumble_menu_quads.cpp src/rumble_play.cpp src/rumble_flow.cpp
  src/rumble_screen.cpp src/rumble_clipless.cpp src/rumble_crates.cpp src/rumble_knives.cpp
  src/rumble_platform.cpp src/rumble_stage_quads.cpp src/rumble_label.cpp src/rumble_coop_script.cpp
  src/rumble_join_report.cpp src/rumble_demo.cpp)
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  like_nes_mobile_app(neon_rumble SOURCES src/rumble_mobile.cpp ${RUMBLE_SOURCES}
    NAME "Neon Rumble" ID com.likenes.neonrumble)
  target_link_libraries(neon_rumble PRIVATE like-nes::engine)
else()
  add_executable(neon_rumble src/main.cpp src/rumble_window.cpp src/rumble_keys.cpp src/rumble_pad_report.cpp
    src/rumble_window_script.cpp src/rumble_shot.cpp
    ${RUMBLE_SOURCES})
  target_link_libraries(neon_rumble PRIVATE like-nes::engine like-nes::window)
  like_nes_add_game(neon_rumble)
endif()
like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle)
```
<!-- /snippet -->

- `find_package(like-nes 0.1 REQUIRED CONFIG)` accepts any 0.1.x. While the major version is 0,
  the API may break between minor versions, so 0.2 is a different package.
- `like-nes::engine` brings the framework, the renderer and the platform layer. `like-nes::window`
  brings the desktop window and native input: the GLFW keyboard and mouse pump and the gamepad
  backend of the OS (`input/source.hpp`, `make_gamepad_source`): GameController on macOS, XInput on
  Windows, evdev on Linux.
- On iOS and Android `like_nes_mobile_app(<target> SOURCES <file>... [BUNDLES <file>...] NAME <name> ID <bundle id>)`
  builds the game on the SDK's mobile shell instead of an executable: an `.app` on iOS, a `.so` and
  the `<target>_apk` target on Android. The game defines `mobile::make_game()`
  (`mobile/mobile_app.hpp`); Neon Rumble's is `src/rumble_mobile.cpp`. `like-nes::window` is
  desktop only, so `main.cpp`, `rumble_window.cpp`, `rumble_keys.cpp` and `rumble_pad_report.cpp` stay in the desktop
  branch.
- `like_nes_add_game(<target>)` makes the game use the same C runtime as the engine. On MSVC the
  engine links the CRT statically, and a Debug game without this call fails to link with
  `LNK2038`. The call also copies the wgpu runtime next to the executable.
- `like_nes_bake(<target> MANIFEST <file> OUT <bundle>)` runs the SDK's `assetc` on the manifest
  and copies the game's bundle and the SDK's `library.bundle` next to the executable. `assetc`
  writes a depfile of every file it read (the manifest, every PNG, every `.tmj` and the `.tsj` it
  references), so editing a tileset re-bakes the bundle on the next build.

Configure and build the game against the prefix:

```sh
cmake -S games/neon-rumble -B build-game -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=../like-nes-sdk
cmake --build build-game
./build-game/neon_rumble --headless --frames 240
```

## The manifest

`game.manifest` lists what goes into `game.bundle`, one record per line. These are the fields of
each line, separated by `|`:

<!-- snippet: games/neon-rumble/game.manifest -->
```text
# What like_nes_bake bakes into game.bundle (spec #24, B4-B5): one record per line,
# texture|<name>|<pixel or hd>|<path from this directory>, level|<name>|tiled|<path>.tmj,
# clips|<name>|aseprite|<path>.json (spec #24, B6) over a sheet that is a texture record by path.
# A trailing |viewport on a level makes the bake prove its layers cover the viewport limit (B7).
# font|<name>|bitmask|<path>.json bakes a glyph atlas and the fonts section, credits|<name>|<path>.txt
# the credits section (B8b). fighter|<name>|<path>.fighter bakes a fighter table (spec #25, B2b).
texture | street_tiles | pixel | assets/warped-city/tileset.png
texture | sign_tiles | pixel | assets/warped-city/signs.png
texture | sky | pixel | assets/warped-city/sky.png
texture | far_city | pixel | assets/warped-city/buildings-bg.png
texture | near_city | pixel | assets/warped-city/near-buildings-bg.png
level | level1 | tiled | levels/level1.tmj | viewport
texture | rainbird_sheet | pixel | assets/puffolotti/rainbird.png
clips | rainbird | aseprite | assets/puffolotti/rainbird.json
texture | banderas_sheet | pixel | assets/puffolotti/banderas.png
clips | banderas | aseprite | assets/puffolotti/banderas.json
texture | adler_sheet | pixel | assets/puffolotti/adler.png
clips | adler | aseprite | assets/puffolotti/adler.json
font | monogram | bitmask | assets/monogram/monogram-bitmap.json
credits | credits | assets/credits.txt
fighter | banderas_fighter | fighters/banderas.fighter
fighter | rainbird_fighter | fighters/rainbird.fighter
fighter | adler_fighter | fighters/adler.fighter
```
<!-- /snippet -->

| Record | Fields | What it bakes |
|---|---|---|
| `texture` | name, `pixel` or `hd`, path to a PNG | one texture. `pixel` is RGBA8 with nearest sampling and no mipmaps. `hd` needs `basisu`, and the SDK does not ship it |
| `level` | name, `tiled`, path to a `.tmj`, optionally `viewport` | a Tiled map: see [Levels in Tiled](tiled-levels.md) |
| `clips` | name, `aseprite` or `sheet`, path | animation clips: see [Animations in Aseprite](aseprite-animations.md) |
| `font` | name, `bitmask`, path to a `.json` | a bitmap font and its atlas: see [Fonts and credits](fonts-and-credits.md) |
| `credits` | name, path to a `.txt` | the credits table: see [Asset licenses](asset-licenses.md) |
| `fighter` | name, path to a `.fighter` | a fighter table: speeds, jump, depth, hp and the strikes of the fighter's clips |

The bake refuses a line instead of guessing what it meant:

- A name is `[A-Za-z0-9_.-]` and is unique in the manifest. `tilemap`, `visual`, `objects`, `clips`
  and `fonts` are reserved, because the bake gives those names to its own tables.
- A path is relative to the manifest and uses `/`. `..`, `\`, `:`, a leading `/` and an empty or
  `.` segment are refused. Every segment must match the directory listing **in case**, on every
  OS. `Tiles/` written for `tiles/` would work on macOS and fail for a player on Linux.
- A texture needs its codec. A missing codec is refused instead of being silently treated as `hd`.
- A texture side is at most 2048 px, the safe limit for wgpu on mobile GPUs.
- `#` starts a comment anywhere on a line, and a UTF-8 BOM is refused.

Every refusal names `<manifest>:<line>` and stops the game's build with that line.

## The bundle hash

The bake is deterministic: the same input gives the same bytes on every OS. `assetc` prints
`[assetc] bundle_hash = 0x…` when it bakes, and Neon Rumble commits the value it expects to
`games/neon-rumble/bundle.hash`. `check_sdk_game.sh` compares the two in Release and Debug. A
different hash on one machine is a finding, not a reason to rewrite the file. The file changes only
in the commit that changes the content or the baker.

## Running the game

`--headless --frames 240` maps the bundle, opens every table and prints a summary of what it read.
That run is what CI checks. Without `--headless` the game opens a 960×540 window that can be
resized, on a menu: **↑** and **↓** select `level`, `wave` or `credits`, **Enter** enters, **Esc**
goes back to the menu from the level. `--level <name>` sets the level of the menu, `--wave` selects
the wave item, and a headless run enters the selected item without the menu
(see [Scenes and levels](scenes-and-levels.md#the-menu-of-neon-rumble)). Banderas is player 1: WASD walk him, Space or K jumps, J jabs, U throws a cross, L kicks
and J in the air jump-kicks. Rainbird is player 2 and comes onto the street when a free pad presses
any button or a key of his layout is pressed: an arrow, numpad 0–6 or numpad −. Backspace takes player 1 off
the keyboard, numpad − player 2, Back on a pad its player. **F3** toggles the debug overlay of the fighters, and **F1** opens the
credits screen and turns its pages; the press after the last page closes it.
`--frames <n>` closes the window after `n` frames with exit code 0. `--headless` needs `--frames`
with `n` above 0, so a headless run always ends.
