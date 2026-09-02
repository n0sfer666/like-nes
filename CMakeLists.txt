cmake_minimum_required(VERSION 3.24)
project(like_nes LANGUAGES C CXX)

# Дерево 64-битное, и это не предпочтение: предсобранный wgpu-native выбирает каталог рантайма по
# процессору ХОСТА, а не по разрядности цели, поэтому 32-битный тулчейн получает x86_64-библиотеку
# и разъезжается с ней на линковке. До линковки он успевает наврать другое: uint64_t → size_t в
# render/capture.cpp под /W4 становится C4244, и причиной выглядит наш код. На Windows такой
# тулчейн выбирается сам собой — «Developer Command Prompt» и «Developer PowerShell» по умолчанию
# 32-битные; нужен «x64 Native Tools Command Prompt for VS».
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "like-nes собирается только под 64 бита, а этот тулчейн 32-битный "
        "(${CMAKE_CXX_COMPILER}).\n"
        "Windows: открой «x64 Native Tools Command Prompt for VS», удали каталог build "
        "и сконфигурируй заново — кеш помнит компилятор.")
endif()

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
# Бэкенд ОКНА этот glue выбирает на этапе компиляции, а `_GLFW_X11`/`_GLFW_WAYLAND` у самой glfw
# объявлены PRIVATE — до потребителя они не доходят, и на Linux ветка всегда бралась X11-ная
# (`#else` в его цепочке). GLFW при этом выбирает платформу В РАНТАЙМЕ, поэтому под Wayland-сессией
# бинарь спрашивал X11-дисплей у Wayland-платформы, получал NULL и умирал паникой wgpu
# «Display pointer is not set» — уже внутри создания поверхности, без единого слова про причину.
# Пробрасываем свой же выбор: каталог, собранный с LINUX_WAYLAND=ON, и поверхность создаёт
# Wayland-ную. Выбор остаётся ОДИН на каталог сборки — ровно поэтому гейт 6 требует двух деревьев.
# Под `Linux`, а не голым `if(LINUX_WAYLAND)`: у самой glfw опция `GLFW_BUILD_WAYLAND` вне Linux
# игнорируется ею же, а у glue такой защиты нет — `-DLINUX_WAYLAND=ON` на macOS увёл бы его в
# Wayland-ветку создания поверхности поверх Cocoa.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LINUX_WAYLAND)
  target_compile_definitions(glfw3webgpu PRIVATE _GLFW_WAYLAND)
endif()
# Публичные макросы — НАШИ и объявляются ЗДЕСЬ, потому что выбор бэкенда окна это выбор ОС, а он по
# инварианту 1 спеки #12 живёт в сборке, не в исходнике: `#if defined(__linux__)` в коде редактора
# гейт `tree_invariants.sh seam` отбивает — и правильно. Потребителю нужно знать, под какую сессию
# собран каталог, чтобы назвать расхождение словами. Чужое `_GLFW_WAYLAND` для этого не годится:
# оно приватное намеренно (внутренний макрос glfw), публиковать его значило бы включить внутренние
# ветки glfw в нашем коде.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(LINUX_WAYLAND)
    target_compile_definitions(glfw3webgpu PUBLIC LIKE_NES_GLFW_WAYLAND)
  else()
    target_compile_definitions(glfw3webgpu PUBLIC LIKE_NES_GLFW_X11)
  endif()
endif()

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
  # flecs header-only для потребителя, и `component.hpp:169` содержит `if (is_empty<T>::value && …)` —
  # для cl.exe это C4127 «условное выражение является константой». Диагностика ЧУЖАЯ, но приезжает
  # как ошибка сборки НАШЕГО файла: инстанцирование шаблона начинается в scene.cpp/serialize.cpp, и
  # ни `/external:I` (заголовок помечен системным), ни `/external:templates-` её не снимают на
  # MSVC 14.44 (VS 2022 17.14) — флаги в командной строке, ошибка на месте. На 14.51 (VS 18,
  # windows-latest) те же флаги её снимают, поэтому CI зелёный, а машина владельца красная:
  # проверять этот класс поломок раннером нельзя вовсе. Поэтому подавление едет
  # INTERFACE-свойством самой зависимости: тот, кто линкует flecs, получает его автоматически, а
  # список потребителей не приходится вести руками. Правит только C4127 и только на MSVC — остальная
  # строгость нашего кода в этих целях остаётся.
  if(MSVC)
    target_compile_options(flecs_static INTERFACE /wd4127)
  endif()
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
add_subdirectory(engine/material)  # материалы (#18) — до render: golden-харнесс их потребитель
add_subdirectory(engine/light)     # свет как данные (#18, вертикаль 3) — до render, его потребителя
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
