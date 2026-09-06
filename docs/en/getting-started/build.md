# Build

English · [Русский](../../ru/getting-started/build.md)

[Prerequisites](prerequisites.md) first — on Windows, from an *x64 Native Tools* prompt.

<!-- container: build -->
```sh
git clone https://github.com/n0sfer666/like-nes.git
cd like-nes
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first configure downloads the dependencies pinned in `CMakeLists.txt` (GLFW, flecs, the WebGPU
backend, Dear ImGui, …), so it needs network access; later configures do not. A full first build is
minutes, not seconds — most of it is those dependencies, and they are not rebuilt afterwards.

## Options that change what gets built

The set of targets is decided by options, not by the build directory. The defaults are what you
want for a first build; the ones worth knowing:

| Option | Default | Effect |
|---|---|---|
| `PLUGIN_UI` | `ON` | The editor (`editor_shell`). Without it the tree configures headless and no editor target exists. |
| `AUDIO_MINIAUDIO` | `ON` | Audio backend. `OFF` leaves the engine's audio seam with a silent device. |
| `PLUGIN_WASM` | `ON` | WebAssembly plugin host. Needs the wasmtime C API in `deps/`. |
| `IDE_POC` | `ON` | Editor foundation: scene document, round-trip, undo — and the `game_platformer` sample. |
| `LINUX_WAYLAND` | `OFF` | Native Wayland backend instead of X11/XWayland. Linux only. |
| `CMAKE_BUILD_TYPE` | — | Set it. An unset type on a single-config generator means no optimisation flags at all. |

A build directory remembers the options it was configured with, so switching them means
reconfiguring the same directory (or using a second one).

## The build gate

Before committing, use the gate rather than a bare build — it fails on warnings too, on the same
settings CI uses:

```sh
bash scripts/build_check.sh
```

Engine code is compiled with `-Wall -Wextra -Werror` (`/W4 /WX` on MSVC), and third-party
dependencies are exempted from it deliberately: they are pinned per commit, and patching them would
break the byte-determinism the asset baker depends on. The gate also greps the build log, because
`-Werror` covers the compiler alone — the MSVC driver, the linker and CMake itself report warnings
past it.

Call it through `bash`, not by its execute bit: on the Windows runner the shell is git-bash, which
does not inherit the bit from the index.

## Next

[First run](first-run.md) — the editor, the sample game and what a failure looks like.
