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
available. The headers a game may include are listed one by one in `cmake/sdk_headers.cmake`. An
engine header missing from that list is internal, and a game that includes it does not compile
against the prefix. That is on purpose.

`bash scripts/check_sdk_game.sh --keep` runs the same six steps, builds Neon Rumble against the
result and keeps everything in `build-sdk-work/`. It is the shortest way to a prefix you can trust.

### A mobile prefix

A phone build needs its own prefix per platform: `ios-sim`, `ios-device` or `android-<abi>`. It is
built in Release only and has no `assetc`, because the baker runs on the build machine: the game
passes the desktop prefix as `LIKE_NES_HOST_PREFIX`, and its `assetc` bakes the bundles. Neon
Rumble bakes with its own tool, `rumble_assetc` (see below), and a phone build cannot run what it
builds for the phone: `bash scripts/rumble_tool.sh <desktop prefix> <dir>` builds the tool for the
build machine against the desktop prefix and prints its path, and the game takes that path as
`-DRUMBLE_ASSETC`. Without it a phone build of Neon Rumble stops at configure with this command. The
paths below must be absolute, because a cross toolchain does not resolve a relative
`like-nes_DIR`. For the iOS simulator:

```sh
cmake -S . -B build-sdk-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-sdk-ios-sim --target like_nes_sdk
cmake --install build-sdk-ios-sim --component sdk --prefix <ios prefix>
bash scripts/rumble_tool.sh <desktop prefix> build-rumble-tool
cmake -S games/neon-rumble -B build-game-ios-sim -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 -Dlike-nes_DIR=<ios prefix>/lib/cmake/like-nes -DLIKE_NES_HOST_PREFIX=<desktop prefix> -DRUMBLE_ASSETC=<the path it printed>
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

The shared source list is in [`sources.cmake`](../../../games/neon-rumble/sources.cmake).
The mobile app and its CPU host test build the same game code from that list.

<!-- snippet: games/neon-rumble/CMakeLists.txt -->
```cmake
cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)
add_subdirectory(sim)

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  like_nes_mobile_app(neon_rumble SOURCES ${RUMBLE_MOBILE_SOURCES} ${RUMBLE_SOURCES}
    NAME "Neon Rumble" ID com.likenes.neonrumble)
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine)
else()
  add_executable(neon_rumble src/main.cpp src/rumble_window.cpp src/rumble_keys.cpp src/rumble_pad_report.cpp
    src/rumble_window_script.cpp src/rumble_shot.cpp
    ${RUMBLE_SOURCES})
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine like-nes::window)
  like_nes_add_game(neon_rumble)
endif()
# Бойцов печёт инструмент игры (спека #29, В2а). Мобильная сборка его не запускает — бинарь хоста
# приходит готовым путём.
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  if(NOT RUMBLE_ASSETC)
    message(FATAL_ERROR "neon_rumble: a mobile build bakes with the host's rumble_assetc; build it with "
                        "'bash scripts/rumble_tool.sh <desktop-prefix> <dir>' and pass -DRUMBLE_ASSETC=<path it prints>")
  endif()
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL "${RUMBLE_ASSETC}")
else()
  add_subdirectory(tools)
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL rumble_assetc)
endif()
```
<!-- /snippet -->

- `find_package(like-nes 0.1 REQUIRED CONFIG)` accepts any 0.1.x. While the major version is 0,
  the API may break between minor versions, so 0.2 is a different package.
- `like-nes::engine` brings the framework, the renderer and the platform layer. `like-nes::window`
  brings the desktop window and native input: the GLFW keyboard and mouse pump and the gamepad
  backend of the OS (`input/source.hpp`, `make_gamepad_source`): GameController on macOS, XInput on
  Windows, evdev on Linux.
  `like-nes::engine` also includes `framework_touch`: layouts and `TouchSource` turn screen
  coordinates into the same action map as the other input devices.
- `add_subdirectory(sim)` adds `rumble_sim`, the fight code without a window. Its build file links
  `like-nes::engine` when the target exists, as it does here, and the engine targets of the source
  tree otherwise: the same library builds inside the engine tree under its strict warnings and
  against the SDK for the game. It sets the static C runtime itself, as `like_nes_add_game` does
  for the game.
- `rumble_sim` also adds the shared platformer kit from `games/kits/platformer/` as
  `kit_platformer`. The kit owns the controller and movement profile and compiles against the SDK;
  the SDK supplies collision, slide, support, push and character state. These kit sources travel
  with the game sources, outside the installed prefix.
- `add_subdirectory(tools)` adds `rumble_assetc`: the SDK's baker core `like-nes::assetc_core` and
  the game's own `fighter` record (`tools/fighter_kind.cpp`), which the SDK's `assetc` does not
  know. A record kind is an `asset::manifest::Kind` from `assetc_kind.hpp`: a name, a file extension
  and a function that turns the file's text into the bytes of a table; the core reads the manifest,
  checks the path and writes the section. The tool's `main` hands its kinds to
  `asset::manifest::run_manifest` from `assetc_manifest_mode.hpp`. The tool links only `like-nes::assetc_core` and
  the game-owned `rumble_fighter` and takes its include paths from `like-nes::engine`, so it
  does not pull the renderer.
- On iOS and Android `like_nes_mobile_app(<target> SOURCES <file>... [BUNDLES <file>...] NAME <name> ID <bundle id>)`
  builds the game on the SDK's mobile shell instead of an executable: an `.app` on iOS, a `.so` and
  the `<target>_apk` target on Android. The game defines `mobile::make_game()`
  (`mobile/mobile_app.hpp`); Neon Rumble's is `src/rumble_mobile.cpp`. `like-nes::window` is
  desktop only, so `main.cpp`, `rumble_window.cpp`, `rumble_keys.cpp` and `rumble_pad_report.cpp` stay in the desktop
  branch.
- `like_nes_add_game(<target>)` makes the game use the same C runtime as the engine. On MSVC the
  engine links the CRT statically, and a Debug game without this call fails to link with
  `LNK2038`. The call also copies the wgpu runtime next to the executable. On MSVC Debug against a
  Release-only prefix it stops the configure through `like_nes_require_debug_libs(<target>)`, which
  a game's own tool built against the prefix calls too.
- `like_nes_bake(<target> MANIFEST <file> OUT <bundle> [TOOL <baker>])` runs a baker on the manifest
  and copies the game's bundle and the SDK's `library.bundle` next to the executable. Without `TOOL`
  the baker is the SDK's `assetc`. `TOOL` is a target of the game's build, which is built first, or
  an absolute path to an existing executable; anything else stops the configure. A path needs
  `<path>.like-nes-build` beside it with the build of this SDK: `rumble_assetc` built against a
  prefix writes it, and a tool built against another build of the engine stops the configure.
  Neon Rumble passes `rumble_assetc` on the desktop and `${RUMBLE_ASSETC}` on a phone. The baker
  writes a depfile of every file it read (the manifest, every PNG, every `.tmj` and the `.tsj` it
  references), so editing a tileset re-bakes the bundle on the next build. Release, RelWithDebInfo
  and MinSizeRel add `--strict`: missing ru translations stop the bake; Debug reports fallbacks.

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
# Global viewport settings: positive integer sides, standard values add no bundle section.
zone | 384 | 216
zone_limit | 576 | 312
touch | touch | layout | controls/touch.txt
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
| `strings` | name, `tsv`, path to a `.tsv`, font name | an LNST translation table; the font may be declared later |
| `music` | name, `vorbis`, path to an `.ogg`, optional loop start/end and `fade_ms=N` | original Ogg bytes and the LNMU `music` table |
| `credits` | name, path to a `.txt` | the credits table: see [Asset licenses](asset-licenses.md) |
| `fighter` | name, path to a `.fighter` | a fighter table: speeds, jump, depth, hp and the strikes of the fighter's clips. A record of the game's tool `rumble_assetc`: the SDK's `assetc` refuses it |

A strings record is `strings|labels|tsv|labels.tsv|monogram`. The literal UTF-8 TSV starts with
exactly `key`, `en`, `ru`, separated by tabs; every data row has those three fields. Spaces are
preserved; quoting and escape sequences are not interpreted. An empty ru value uses en and adds
one fallback to `[assetc] strings labels: keys=N fallbacks=M`. `--strict` refuses any fallback;
an explicit ru value identical to en is complete. Every code point in both values must exist in
the named font, including alphabets stored under `grid.ip`, `grid.name` and `grid.room`. A refusal
names the key and `U+XXXX`. Keys use `[A-Za-z0-9_.-]`, at most 128 bytes; duplicate keys or FNV ID
collisions are refused. A table holds up to 4096 keys; source TSV and encoded table each cap at 16 MiB.

At runtime, open the named raw section with `framework::ui::Strings` from `strings.hpp`, then
call `find(key, Language::En)` or `Language::Ru`; a missing key returns an empty view. `font_name()`
identifies the linked font. The aligned source buffer must remain alive and unchanged: the
reader and returned views borrow it. Opening and lookup allocate no heap memory.

`framework::ui::UiInputChannel` (`ui_input.hpp`) is a separate menu channel. Bake an input
preset named `ui` with nonempty actions `up`, `down`, `left`, `right`, `accept`, `back`, `erase`;
optional axes are named `x` and `y`. Call `init(PresetTable)` outside the frame. It copies the
bindings (up to 256) and shapes; the original table may then be released. For example, accept
can bind `key:enter|pad:south`, back `key:esc|pad:east`, and erase `key:backspace|pad:west`.

After the game's single input drain, call `read(engine.device(), elapsed_ms, pointers, text,
text_source)` once per UI frame. All connected pads and the keyboard participate together,
independently of player assignments; the channel never drains or consumes game input. A stick
axis triggers at half the shaped range; paired x/y use radial response (x's shape when both
pair declarations exist), other axes use axial response. Opposing active directions cancel.
Navigation and erase repeat after 350 ms, then every 90 ms, at most once per frame. Accept/back use
press edges. Pointer coordinates are zone pixels; IDs retain the full intptr_t width. There are
16 pointer events and 64 text code points per frame; overflow or invalid pointer data cancels it.

The default `PhysicalLatin` text source reads key edges as A–Z, digits, space, `.`, `:` via Shift
and semicolon, and keypad digits/decimal. `Native` uses the supplied Unicode code points instead;
invalid/control scalars are discarded. Call `cancel()` on screen transitions, focus loss or
suspend: held buttons and navigation axes must release before acting again. read/cancel allocate
no heap memory.

`framework::ui::Context` (`ui_context.hpp`) builds fixed-capacity `render::Quad` and `QuadRun`
spans without a GPU dependency. `init(strings, fonts, font_texture, solid_texture)` selects the
font named by the strings table. Font and string buffers must stay alive and unchanged; the
reader objects themselves need not. Register a white 1×1 texture for solid panels and the font
atlas with your renderer, then pass their material indices. Quad/QuadRun layout is unchanged;
`render_types` exposes these CPU types without linking the renderer.

Call `begin(UiInput, UiView)`, then `button`, `slider`, `field` or `list`, and `end()`. UiView gives
zone width/height, integer scale 1–64 and screen origin. Rectangles use zone pixels. `end()` returns
borrowed spans valid until the next begin/init or Context destruction; upload and draw them in
run order. Static labels are string keys; `set_language` selects en/ru. `ui_id(parent, key, index)`
uses common FNV; use distinct parents for screens and an index for repeated items. ID zero is valid.

Up/down move focus through enabled widgets in declaration order, wrapping; x adjusts a focused
slider. Activation and slider input require the widget to have appeared in the previous finished
UI frame. Pointer hits use that frame's geometry, primary-pointer capture and release inside the
visible widget; a painted disabled widget blocks click-through. Resizing cancels the old pointer
geometry. The game owns screen transitions and unconsumed `end().back`; cancel the input channel
when changing screens or suspending. Use a separate Context or disable underlying controls for
modal overlays. A field displays its NUL-terminated caller buffer (termination scan capped at 1025
bytes) and returns activation; it does not edit it. A slider changes caller data only on input,
not while drawing or cancelling. A list fills its rectangle with vertical rows.

There are 128 widgets, 2048 quads and 512 runs per frame. Duplicate IDs and invalid rectangles are
refused; missing labels and fully clipped widgets cannot receive input. UTF-8 text is bounded to
256 code points per draw and clipped with adjusted UVs. Stats report clipping, invalid data and
capacity drops; an incompletely drawn widget is disabled and slider changes are rolled back.
All widget methods and overflow paths allocate no heap memory.

`glyph_grid(parent, alphabet_key, buffer, max_len, rect, columns, labels)` edits a mutable
NUL-terminated UTF-8 buffer. The alphabet is a Strings key (for example `grid.ip`, `grid.name`,
`grid.room`), so both translations pass font coverage at bake time. There are at most 126 unique
printable Unicode scalars, plus erase/done cells. `max_len` counts scalars; buffer capacity includes
the terminator and is capped at 1025 bytes. Invalid UTF-8 or missing termination refuses editing.
Existing over-limit text stays intact and can be erased; an append needs both scalar and byte space.

D-pad/stick x/y move within the grid, clamping row/column; accept or a tap enters the selected
scalar. Native keyboard code points must match the alphabet exactly (no case folding). Erase
removes one complete UTF-8 scalar. The erase/done labels default to Strings keys `grid.erase` and
`grid.done`; override their keys through `UiGridLabels`. Optional preset action `done` can bind
`pad:start`; it is edge-triggered and respects cancellation. The done cell also works without it.
A focused grid returns `back` while preserving the buffer and consumes `end().back`; the game
owns the next screen. Map physical B to `back` or `erase` in the ui preset to choose its behavior.
Back/done take priority over edits in the same frame. Results report changed/done/back/full,
invalid input and rejected code points.

A grid must fit entirely inside the zone and have enough widget/quad capacity. A partially drawn
grid cannot edit data; a fully rendered frame must precede input. Character IDs include their
code point, preserving identity when a translated alphabet reorders characters. Use a distinct parent for every grid in a frame; a reused parent is refused even with another
alphabet key. Draw the grid before its field to display the edited text in the same frame.

Static screen text must come from Strings. Run `python3 scripts/check_ui_text.py --strings path/strings.tsv --screens path/screen.cpp path/pause.cpp` with explicit screen sources.
The gate refuses unknown/direct visible literals, including aliases, raw strings, concatenation
and printable character literals. Use literal known keys directly at UI calls, explicit
`UiListItem{"machine-id", "label.key"}` and `UiGridLabels{"grid.erase", "grid.done"}`.
Includes, empty buffer seeds and direct printf/fprintf/assert diagnostics are allowed; a nested
UI call inside a diagnostic keeps its own checks. There are no per-line suppressions. Local macros overriding registry names disable their
exemptions; Unicode identifiers cannot disguise a registry name. Backslash-LF/CRLF continuations
are refused throughout screen sources, including raw literals and comments.
This conservative syntax guard does not follow arbitrary C++ data flow or inspect other units;
review still checks generated/dynamic text and function/type shadowing of registry names. Empty scans refuse. `--selftest` runs the #26 fixtures;
pilot #29 adds actual screen sources. Details: `.context/gates/ui-text.md`.

Music metadata lives in `framework::music::MusicTable` (`music_read.hpp`) and LNMU v1.
The aligned immutable buffer must outlive the reader and its views; open/find allocate no heap.
`music_bake.hpp` bakes `MusicSrc` records from inspected source metadata: rate 48000 Hz,
mono/stereo, a nonempty frame range and `0 <= loop_start < loop_end <= frames`.
An omitted loop end uses the complete source. `fade_ms` defaults to 1000 and converts exactly
to sample frames; zero selects an instant transition, overflow refuses. Names are ASCII machine
keys up to 128 bytes; IDs use common FNV. Rows and the string pool are canonical by ID, with
4096 tracks and a 1 MiB section cap. Refused baking preserves the previous output.
The manifest accepts `music|menu|vorbis|assets/menu.ogg[|loop_start|loop_end][|fade_ms=N]`.
The optional named fade suffix comes last and may appear without loop points. The importer
fully decodes the source to validate its frame count, requires 48000 Hz mono/stereo, and refuses
an out-of-range loop. Positions count sample frames, not interleaved values. Each encoded file
is capped at 64 MiB and stored unchanged as Audio/Vorbis/Mmap, with rate/channels/frames metadata.
All music records share one canonical raw table named `music`; it is absent when there are no
music records. Refusal leaves an existing output bundle and depfile unchanged.

The SDK exports `audio_core` through `like-nes::engine`: `VorbisReader` (`vorbis_reader.hpp`)
borrows immutable encoded bytes until close/destruction. Each instance belongs to one thread;
different instances may decode in parallel. All decoding/open/seek work runs outside the audio
callback. Reads accept native channels or stereo; EOF succeeds with zero frames. Full mono
`decode_vorbis` remains available for effects. Mixer stereo PCM is immutable and must outlive
the Mixer, after the callback has stopped and joined. `StereoRing` (`audio_stereo_ring.hpp`) supplies fixed storage for16384stereo frames
(341ms at48000Hz). Allocate it before callbacks and retain it until producer and consumer join.
One producer pushes nonzero64bit generation tags and receives the actual partial write count;
one consumer takes its expected generation, discarding other tags. Each call scans at most the
ring capacity; a miss returns false and a zero frame. The caller counts underruns. No cursor reset,
heap allocation or lock is involved. This is a stream primitive; worker and playback transitions
are the next part of #26 V6.

`MusicIntent` (`music_intent.hpp`) observes a complete `(simulation_tick, track_id)` pair outside
rollback snapshots. Pass `framework::rollback::SETTLE_TICKS<Depth>` or checked `settle_ticks(depth)`
from `rollback_timing.hpp`; depth8 gives25. Runtime overflow returns zero, an invalid/inert intent.
Only unique forward ticks count; replayed/duplicate ticks cannot change even the candidate.
A gap or a different forward wish restarts the count. Tick comparison handles uint32 wrap with
modular distance below half the range. At the exact threshold, observe returns the new stable
64bit track ID once; zero is a valid ID. `stable_track()` retains the last settled selection while
a new wish settles. The count follows each tick's first forward observation; historical corrections
stay in Session. One owning thread uses one continuous tick epoch; create a new intent after
Session::reset/restart. This selects an intent, not an audible track; playback remains a separate step.

`MusicTrack` (`music_track.hpp`) holds cold source values for the upcoming stream: a64bit ID,
exactly one immutable encoded Ogg or stereo/48000Hz PCM span, the full frame count, loop start/end
and fade length in samples (default48000). Both spans borrow storage until the worker stops and
joins. The intro starts at0, then loops from the exclusive end to loop_start. The worker decoder
uses the same bounded loop path for Ogg and PCM; a failed block is discarded in full.

The bake refuses a line instead of guessing what it meant:

- A name is `[A-Za-z0-9_.-]` and is unique in the manifest. `tilemap`, `visual`, `objects`, `clips`
  and `fonts` are reserved, because the bake gives those names to its own tables. When any music
  records are present, `music` is also reserved for the LNMU table.
- A path is relative to the manifest and uses `/`. `..`, `\`, `:`, a leading `/` and an empty or
  `.` segment are refused. Every segment must match the directory listing **in case**, on every
  OS. `Tiles/` written for `tiles/` would work on macOS and fail for a player on Linux.
- A texture needs its codec. A missing codec is refused instead of being silently treated as `hd`.
- A texture side is at most 2048 px, the safe limit for wgpu on mobile GPUs.
- `#` starts a comment anywhere on a line, and a UTF-8 BOM is refused.

Every refusal names `<manifest>:<line>` and stops the game's build with that line.

## The bundle hash

The bake is deterministic: the same input gives the same bytes on every OS. The baker prints
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

The mobile build defaults to touch controls in the training arena on the level 1 map. `--wave`
enables its existing progression; `--demo` explicitly selects scripted parity input. See
[Touch controls on a phone](mobile-touch.md) for a fresh interactive
launch, the five combat buttons and the physical layout check.

## Save files

`framework_save` exports `save.hpp`. Initialize it on the game thread with `save::init` and the
canonical application ID (the same syntax as slot names) before using the global `load`/`store` functions. The shell's explicit save
directory takes priority, including an empty refusal. Otherwise a nonempty absolute
`LIKENES_SAVE_DIR` overrides the desktop OS data directory. An unavailable or relative override
refuses persistence; it never falls back to the executable or working directory. A later write
retries the same intended directory. `save::dir` exposes the active root; `save::file_path`
prepares it and validates a compatibility filename. ACH and controls retain their existing formats.

Slots use lowercase ASCII letters, digits, `_` and `-`, up to 63 bytes, excluding DOS device
names. Serialize operations on a slot and its backups. LNSV bodies are limited to 16 MiB.
`load` requires exactly one migration callback per version step from version 1 to the requested
version; callbacks must not throw. It preserves an original `.pre` before migration, puts future
data in `.bak` and corrupt data in `.bad`, choosing a free suffix without overwriting a backup.
The global overload returns an absolute quarantine path. Invalid arguments and IO failures are
distinct from corruption. New writes continue the largest readable slot/backup counter; overflow
refuses the write. `save_backend.hpp` also offers `select_record` for replica selection and
`LocalBackend` for the explicit backend overloads; equal counters with different valid images
yield `Conflict` and preserve both.
