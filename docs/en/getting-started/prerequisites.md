# Prerequisites

English · [Русский](../../ru/getting-started/prerequisites.md)

Everything the build needs beyond a compiler is fetched by CMake (`FetchContent`). The lists below
are what the *system* must provide. Package names are Debian/Ubuntu; the Fedora/Nobara equivalents
follow in the second one-liner.

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

One line for Fedora — and for Nobara, which is Fedora underneath, so nothing here is special-cased
for it:

```sh
sudo dnf install -y gcc-c++ cmake ninja-build git python3 \
    libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libxkbcommon-devel \
    mesa-libGL-devel vulkan-loader vulkan-tools mesa-vulkan-drivers
```

Fedora has no `xorg-dev` meta-package, hence the spelled-out X11 `-devel` list; `mesa-vulkan-drivers`
there already contains radv, anv **and** lavapipe, so no per-vendor package is needed.
`mesa-libGL-devel` is in that list even though nothing here draws with OpenGL: `glfw3.h` includes
`GL/gl.h` on X11 regardless of `GLFW_CLIENT_API`, and on Debian `xorg-dev` drags the header in as a
dependency, which is why the Ubuntu line never had to name it. Missing it fails the build of the
`glfw3webgpu` dependency, not of engine code. If GLFW ever asks for more than the list above,
`sudo dnf builddep glfw` (needs `dnf-plugins-core`) installs exactly what Fedora builds GLFW with.
On Nobara, upgrade the system with `nobara-sync cli` rather than `dnf upgrade` — it substitutes some
Fedora packages with its own and a plain upgrade reverts them; installing individual packages with
`dnf` is fine.

**Wayland is opt-in.** GLFW is built X11-only by default, so on a Wayland session the editor runs as
an XWayland client. To exercise the native Wayland backend, add `libwayland-dev`, `libxkbcommon-dev`
and `wayland-protocols` (which brings `wayland-scanner`) — on Fedora that is `wayland-devel` and
`wayland-protocols-devel` — then configure with `-DLINUX_WAYLAND=ON`.

## Windows: which shell

Run every command from an *x64 Native Tools Command Prompt for VS*, or from a shell where
`vcvars64.bat` has been sourced. Plain `cmd`/PowerShell has no `cl.exe` on `PATH`, and CMake falls
back to whatever compiler it finds — or to none.

The name matters past `cl.exe` being present: the unqualified *Developer Command Prompt* and
*Developer PowerShell* set up the **32-bit** toolchain, and configure refuses it. The tree is 64-bit
only, because the prebuilt wgpu-native picks its runtime directory by host CPU and hands a 32-bit
build the x86_64 library.

The short path is [`scripts/win-dev.bat`](../../../scripts/win-dev.bat), which works from any shell:
`win-dev.bat setup` installs the toolchain through winget (Build Tools rather than the IDE — same
compiler, a quarter of the disk), and `win-dev.bat build` locates the installation with `vswhere`,
sources `vcvars64.bat` and builds.

## Next

[Build the engine](build.md) → [first run](first-run.md).
