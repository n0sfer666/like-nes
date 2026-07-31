# First run: clone → build → editor

Verified path from a clean machine to an open editor on macOS, Linux and Windows. Everything the
build needs beyond a compiler is fetched by CMake (`FetchContent`) — the lists below are what the
*system* must provide. Installing them automatically is out of scope here (spec #20).

## What you need per OS

| | macOS | Linux (Ubuntu/Debian LTS) | Windows |
|---|---|---|---|
| Compiler | Xcode Command Line Tools (`xcode-select --install`) | `build-essential` (gcc) or `clang` | Visual Studio 2022+ with the *Desktop development with C++* workload |
| Build tools | `cmake`, `ninja` (`brew install cmake ninja`) | `cmake`, `ninja-build` | `cmake`, `ninja` (both ship with the VS workload) |
| Windowing | — (Cocoa) | `xorg-dev` | — (Win32) |
| GPU | Metal, nothing to install | `mesa-vulkan-drivers`, `libvulkan1`; `vulkan-tools` to diagnose | vendor driver (DX12) |
| Git | any | any | any |

One line for Ubuntu:

```sh
sudo apt-get install -y build-essential cmake ninja-build git xorg-dev mesa-vulkan-drivers libvulkan1 vulkan-tools
```

**Wayland is opt-in.** GLFW is built X11-only by default, so on a Wayland session the editor runs
as an XWayland client. To exercise the native Wayland backend, add `libwayland-dev`,
`libxkbcommon-dev` and `wayland-protocols` (which brings `wayland-scanner`), then configure with
`-DLINUX_WAYLAND=ON`.

On Windows the short path is [`scripts/win-dev.bat`](../scripts/win-dev.bat), which works from any
shell: `win-dev.bat setup` installs the toolchain through winget (Build Tools rather than the IDE —
same compiler, a quarter of the disk), `win-dev.bat build` locates the installation with `vswhere`,
sources `vcvars64.bat` and builds. The rest of this section is that done by hand.

On Windows, run the commands below from an *x64 Native Tools Command Prompt for VS* (or a shell
where `vcvars64.bat` has been sourced): plain `cmd`/PowerShell has no `cl.exe` on `PATH`, and CMake
will fall back to whatever compiler it finds — or none. The name matters past `cl.exe` being
present: the unqualified *Developer Command Prompt* and *Developer PowerShell* set up the 32-bit
toolchain, and configure refuses it — the tree is 64-bit only, because the prebuilt wgpu-native
picks its runtime directory by host CPU and hands a 32-bit build the x86_64 library.

## Build

```sh
git clone https://github.com/n0sfer666/like-nes.git
cd like-nes
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first configure downloads the dependencies pinned in `CMakeLists.txt` (GLFW, flecs, WebGPU
backend, Dear ImGui, …), so it needs network access; later configures do not. A full first build is
minutes, not seconds — most of it is the dependencies, and they are not rebuilt afterwards.

Before committing, use the gate instead of a bare build — it fails on warnings too:

```sh
bash scripts/build_check.sh
```

## Open the editor

```sh
./build/editor_shell          # Windows: build\editor_shell.exe
```

The editor target exists only when `PLUGIN_UI` is `ON` (the default). If `build/editor_shell` is
missing, the tree was configured headless — reconfigure with `-DPLUGIN_UI=ON`.

## Edit → build → hot-reload

The loop the editor drives is covered end to end by a gate you can run yourself:

```sh
./build/build_loop_test       # writes a .cpp, builds it with your compiler, reloads it
```

It prints `ide-build-loop: PASS` when a source edit reaches a loaded module without restarting the
host — the same path the editor uses when you save a gameplay file.

## When something is missing

Missing system packages are meant to surface as a named error, not as a link failure:

- No Vulkan driver on Linux → the renderer reports that no adapter was found. Check with
  `vulkaninfo | head`; on a headless box `mesa-vulkan-drivers` (lavapipe) is enough to get pixels.
- No X11 development headers → GLFW fails at *configure* time, naming the missing package.
- `cl.exe` not found on Windows → the shell is not a developer prompt (see above).
- `like-nes собирается только под 64 бита` on Windows → it is a developer prompt, but the 32-bit
  one; reopen as *x64 Native Tools* and delete the build directory, whose cache remembers the
  compiler.

## Related

- [`owner-verification.md`](owner-verification.md) — the gates a CI runner cannot close (real GPU
  session, real gamepad) and `scripts/owner_check.sh`, which closes their automated half.
- `CONTRIBUTING.md` — branches, DCO sign-off, what to run before a PR.
- `.context/checks.md` — the checks CI and the pre-commit hook run.
- `.context/env.md` — environment variables the runtime and the gates understand.
