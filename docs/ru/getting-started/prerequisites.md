<!-- en-sha256: 89032647df82c21c35e84ab4fac531ec45624e08225136d691706fbad2803a05 -->

# Системные требования

[English](../../en/getting-started/prerequisites.md) · Русский

Всё, что нужно сборке помимо компилятора, CMake выкачивает сам (`FetchContent`). Списки ниже — то,
что обязана дать *система*. Имена пакетов — Debian/Ubuntu; эквиваленты для Fedora/Nobara идут
вторым однострочником.

| | macOS | Linux (Ubuntu/Debian LTS) | Windows |
|---|---|---|---|
| Компилятор | Xcode Command Line Tools (`xcode-select --install`) | `build-essential` (gcc) или `clang` | Visual Studio 2022+ с нагрузкой *Desktop development with C++* |
| Сборка | `cmake`, `ninja` (`brew install cmake ninja`) | `cmake`, `ninja-build` | `cmake`, `ninja` (оба идут с нагрузкой VS) |
| Окна | — (Cocoa) | `xorg-dev` | — (Win32) |
| GPU | Metal, ставить нечего | `mesa-vulkan-drivers`, `libvulkan1`; `vulkan-tools` для диагностики | драйвер вендора (DX12) |
| Git | любой | любой | любой |

Одной строкой для Ubuntu:

```sh
sudo apt-get install -y build-essential cmake ninja-build git xorg-dev mesa-vulkan-drivers libvulkan1 vulkan-tools
```

Одной строкой для Fedora — и для Nobara, которая под капотом Fedora, поэтому ничего особенного для
неё здесь нет:

```sh
sudo dnf install -y gcc-c++ cmake ninja-build git python3 \
    libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libxkbcommon-devel \
    mesa-libGL-devel vulkan-loader vulkan-tools mesa-vulkan-drivers
```

Мета-пакета `xorg-dev` в Fedora нет — отсюда выписанный список X11 `-devel`; `mesa-vulkan-drivers`
там уже содержит radv, anv **и** lavapipe, так что пакет под конкретного вендора не нужен.
`mesa-libGL-devel` стоит в списке, хотя OpenGL здесь никто не рисует: `glfw3.h` на X11 включает
`GL/gl.h` независимо от `GLFW_CLIENT_API`, а в Debian `xorg-dev` тянет этот заголовок зависимостью —
поэтому строке для Ubuntu называть его не приходилось. Без него падает сборка зависимости
`glfw3webgpu`, а не кода движка. Если GLFW когда-нибудь попросит больше перечисленного,
`sudo dnf builddep glfw` (нужен `dnf-plugins-core`) поставит ровно то, чем Fedora собирает GLFW.
На Nobara систему обновляют командой `nobara-sync cli`, а не `dnf upgrade`: она подменяет часть
пакетов Fedora своими, и обычное обновление их откатывает; ставить отдельные пакеты через `dnf`
можно.

**Wayland — по явному включению.** GLFW по умолчанию собирается только под X11, поэтому в сессии
Wayland редактор работает как клиент XWayland. Чтобы проверить нативный бэкенд Wayland, добавьте
`libwayland-dev`, `libxkbcommon-dev` и `wayland-protocols` (он приносит `wayland-scanner`) — в
Fedora это `wayland-devel` и `wayland-protocols-devel` — и сконфигурируйте с `-DLINUX_WAYLAND=ON`.

## Windows: из какой оболочки

Все команды запускаются из *x64 Native Tools Command Prompt for VS* либо из оболочки, где выполнен
`vcvars64.bat`. В обычных `cmd`/PowerShell нет `cl.exe` в `PATH`, и CMake сваливается на тот
компилятор, который найдёт, — или ни на какой.

Название важно не только из-за наличия `cl.exe`: безымянные *Developer Command Prompt* и *Developer
PowerShell* поднимают **32-разрядный** тулчейн, и конфигурация его отвергает. Дерево только
64-разрядное, потому что предсобранный wgpu-native выбирает каталог рантайма по CPU хоста и отдаёт
32-разрядной сборке библиотеку x86_64.

Короткий путь — [`scripts/win-dev.bat`](../../../scripts/win-dev.bat), он работает из любой
оболочки: `win-dev.bat setup` ставит тулчейн через winget (Build Tools вместо IDE — тот же
компилятор, четверть диска), а `win-dev.bat build` находит установку через `vswhere`, выполняет
`vcvars64.bat` и собирает.

## Дальше

[Сборка движка](build.md) → [первый запуск](first-run.md).
