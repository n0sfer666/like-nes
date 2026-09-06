# First run

English · [Русский](../../ru/getting-started/first-run.md)

You have [built the tree](build.md). This page is the shortest path from that to something on
screen, plus what the common failures look like.

## Open the editor

```sh
./build/editor_shell          # Windows: build\editor_shell.exe
```

Every command below is written for a POSIX shell; on Windows the Visual Studio developer prompt
runs the same binaries under `build\` with an `.exe` suffix, and each block names that form rather
than leaving you to derive it.

The target exists only when `PLUGIN_UI` is `ON` (the default). If `build/editor_shell` is missing,
the tree was configured headless — reconfigure with `-DPLUGIN_UI=ON`.

## Run the sample game

Two samples ship with the engine, and they exercise different halves of it.

```sh
cmake --build build --target game_sidescroller
./build/game_sidescroller     # Windows: build\game_sidescroller.exe
```

`game_sidescroller` is the asset-and-audio sample: it loads a baked bundle, so its first lines name
what came up, and each of them is a check in itself —

```
[game] assets: baked bundle .../game.bundle (BC7 512x256)
[game] materials: on (3 pipeline(s), 0 fallback(s))
```

```sh
cmake --build build --target game_platformer
./build/game_platformer       # Windows: build\game_platformer.exe
```

`game_platformer` is the gameplay sample: a 960x720 window over a 320x240 view at scale 3, with a
640x240 level — wider than the view on purpose, because a camera with nowhere to scroll proves
nothing about its clamp. There is no art in it, only flat quads.

## Edit → build → hot-reload

The loop the editor drives is covered end to end by a gate you can run yourself:

```sh
./build/build_loop_test       # Windows: build\build_loop_test.exe
```

It writes a `.cpp`, builds it with your compiler and reloads it, printing `ide-build-loop: PASS`
when a source edit reaches a loaded module without restarting the host — the same path the editor takes when you save a gameplay file.

## When something is missing

Missing system packages surface as a named error, not as a link failure:

- **No Vulkan driver on Linux** → the renderer reports that no adapter was found. Check with
  `vulkaninfo | head`; on a headless box `mesa-vulkan-drivers` (lavapipe) is enough to get pixels.
- **No X11 development headers** → GLFW fails at *configure* time, naming the missing package.
- **`fatal error: GL/gl.h`** while building the `glfw3webgpu` dependency → install
  `mesa-libGL-devel` on Fedora/Nobara. No reconfigure needed afterwards — just build again.
- **`cl.exe` not found** on Windows → the shell is not a developer prompt; see
  [prerequisites](prerequisites.md#windows-which-shell).
- **`like-nes собирается только под 64 бита`** on Windows → it *is* a developer prompt, but the
  32-bit one. Reopen as *x64 Native Tools* and delete the build directory, whose cache remembers the
  compiler.

## Where to go next

- [Documentation index](../index.md) — what else is written, and what is not written yet.
- [`CONTRIBUTING.md`](../../../CONTRIBUTING.md) — branches, DCO sign-off, what to run before a PR.
