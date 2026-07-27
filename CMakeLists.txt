cmake_minimum_required(VERSION 3.24)
project(like_nes LANGUAGES C CXX)

# Objective-C/C++ нужны только на Apple (Metal-слой); на MSVC/gcc их нет.
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Исходники дерева — UTF-8 без BOM, и комментарии с диагностикой в них по-русски. Без /utf-8 MSVC
# читает файл в системной ANSI-кодировке: на C4819 он ещё только ругается, но байты, которых в
# CP-1252 нет вовсе (`Ё`, `А`, `Н` — 0x81/0x8D/0x90), молча подменяются, и строка «изолирован»
# уезжает в мусор. Спека #12 требует UTF-8 внутри движка — это её выполнение, а не косметика.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Сторонние зависимости (zstd и пр.) объявляют старый cmake_minimum_required (<3.10) → CMake-варнинг
# «deprecated». Поднимаем эффективную минимальную политику зависимостей до 3.10 (CMake ≥3.31; на старом
# CMake переменная игнорируется). Нашу планку 3.24 не трогает — это пол для деп-сабпроектов.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)

# P0-P2 воспроизводимый билд (детерм.-флаги, path-remap, детерм. архивы). Гейт 1 спеки #8.
# Включается ДО add_subdirectory: add_compile_options/add_link_options наследуются вниз.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/determinism.cmake)

option(AUDIO_MINIAUDIO "Real audio device via miniaudio (local; OFF в headless CI)" ON)
option(INPUT_NATIVE "Build native input demo (GLFW + platform gamepad backend)" ON)
option(PLUGIN_UI "Build live plugin UI-shell (Dear ImGui docking + GLFW + WebGPU)" ON)
option(PLUGIN_WASM "Build WASM-sandbox gate (wasmtime C-API from deps/)" ON)
option(IDE_POC "Build IDE editor foundation (flecs scene-doc + round-trip + undo)" ON)
option(LINUX_WAYLAND "Build GLFW with the Wayland backend too (needs wayland-scanner, libwayland-dev, libxkbcommon-dev)" OFF)

include(FetchContent)

# --- GLFW (windowing) ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# По умолчанию X11-only: wayland-scanner и libwayland-dev на раннере не стоят, а рендер в CI —
# xvfb/X11. Wayland включается ЯВНО (`-DLINUX_WAYLAND=ON`), и без этого гейт 6 спеки #13 проверить
# нечем: под Wayland-сессией собранный без него GLFW живёт клиентом XWayland, то есть отвечает на
# вопрос про X11 второй раз. Системные зависимости — docs/first-run.md.
set(GLFW_BUILD_WAYLAND ${LINUX_WAYLAND} CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG a74efa0d5628b74adc0426af4c5710e287fa7c2c  # 3.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# --- WebGPU backend ---
# Движок линкует wgpu-native (prebuilt) — Dawn-из-исходников не собирается на arm64-macOS
# (баг Abseil ×clang21: -msse4.1). Тот же стандартный webgpu.h, что и Dawn (продакшн-цель
# в ADR): render-код переносится без изменений, свап на Dawn — позже.
set(WEBGPU_BACKEND "WGPU" CACHE STRING "" FORCE)
set(WEBGPU_BUILD_FROM_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG f2b81861a0c1889ef51b79fc777da8e11c37bdb3  # main-v0.2.0
)
FetchContent_MakeAvailable(webgpu)

# --- glfw3webgpu (surface creation glue) ---
FetchContent_Declare(glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG 39a80205998c6cbf803e18e750d964c23a47ef39  # v1.2.0
)
FetchContent_MakeAvailable(glfw3webgpu)

# --- stb (PNG read/write для golden-image) ---
# Репозиторий без CMakeLists — MakeAvailable только выкачивает исходники (stb_SOURCE_DIR).
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb
  GIT_TAG f0569113c93ad095470c54bf34a17b36646bbbb5
)
FetchContent_MakeAvailable(stb)

# --- basis_universal transcoder (KTX2/UASTC → BC7 в рантайме, спека #5) ---
# SOURCE_SUBDIR указывает в несуществующий подкаталог → MakeAvailable ТОЛЬКО выкачивает
# исходники (не билдит их CLI). Используем один файл транскодера. Пин по тегу (детерм.).
FetchContent_Declare(basisu
  GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal
  GIT_TAG 323239a6a5ffa57d6570cfc403be99156e33a8b0  # v1_60
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(basisu)

# zstd для bulk-кодека (decompress → арена). Desktop: brew/apt libzstd.
# zstd ПИННУТ из исходников (не системный) → байт-детерминизм бейка cross-machine
# (спека #5: pinned codec versions). Системный libzstd варьируется по версиям меж раннерами.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
  GIT_REPOSITORY https://github.com/facebook/zstd
  GIT_TAG 35016bc1c0b9a2f7121b7ecc312100aad7d9f2ad  # v1.5.6
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(zstd)

# --- miniaudio (AudioDevice HAL-бэкенд, спека #3) ---
# Single-header (как stb): SOURCE_SUBDIR в несуществующий подкаталог → MakeAvailable ТОЛЬКО
# выкачивает исходники. Реальное устройство — локально; в headless-CI выключается опцией.
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio
  GIT_TAG 4a5b74bef029b3592c54b6048650ee5f972c1a48  # 0.11.21
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(miniaudio)

if(PLUGIN_UI)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 97fa363adb1439d11b613d96bcda86d44ea16f4e)  # v1.91.5-docking
  FetchContent_MakeAvailable(imgui)
endif()

# flecs = ECS-мир + meta/json рефлексия (единый источник: property-grid + IPC-зеркало + save/load).
if(IDE_POC)
  set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
  set(FLECS_STATIC ON CACHE BOOL "" FORCE)
  set(FLECS_PIC ON CACHE BOOL "" FORCE)
  set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8  # v4.1.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(flecs)
endif()

# Кросс-платформенно: input_core/audio/game линкуют Threads::Threads на всех ОС
# (на Windows find_package(Threads) даёт no-op imported-target).
find_package(Threads REQUIRED)

# Строгие варнинги — строго между зависимостями и нашим кодом: выше по файлу их уже не достать,
# ниже — они обязательны. Порядок здесь несущий, а не косметический.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/warnings.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(engine/core)
add_subdirectory(engine/platform)   # платформенный шов — до потребителей (asset/plugin/hotreload)
add_subdirectory(engine/render)
add_subdirectory(engine/asset)
add_subdirectory(engine/audio)
add_subdirectory(engine/input)
add_subdirectory(engine/framework) # слой поверх подсистем — после них, зависимость только вниз
add_subdirectory(engine/plugin)
add_subdirectory(engine/achievements)
add_subdirectory(engine/hotreload)
add_subdirectory(tools/assetc)
add_subdirectory(tools/ide)
add_subdirectory(example_ugly_game)
