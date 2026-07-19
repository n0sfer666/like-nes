cmake_minimum_required(VERSION 3.24)
project(like_nes_poc LANGUAGES C CXX)

# Objective-C/C++ нужны только на Apple (Metal-слой); на MSVC/gcc их нет.
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# --- GLFW (windowing) ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# X11-only: избегаем зависимости от wayland-scanner на Linux-CI (рендер в CI — xvfb/X11).
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# --- WebGPU backend ---
# PoC линкует wgpu-native (prebuilt) — Dawn-из-исходников не собирается на arm64-macOS
# (баг Abseil ×clang21: -msse4.1). Тот же стандартный webgpu.h, что и Dawn (продакшн-цель
# в ADR): render-код переносится без изменений, свап на Dawn — позже.
set(WEBGPU_BACKEND "WGPU" CACHE STRING "" FORCE)
set(WEBGPU_BUILD_FROM_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG main-v0.2.0
)
FetchContent_MakeAvailable(webgpu)

# --- glfw3webgpu (surface creation glue) ---
FetchContent_Declare(glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(glfw3webgpu)

add_executable(poc src/main.cpp)
target_link_libraries(poc PRIVATE glfw webgpu glfw3webgpu)
target_copy_webgpu_binaries(poc)

# --- stb (PNG read/write для golden-image) ---
# Репозиторий без CMakeLists — MakeAvailable только выкачивает исходники (stb_SOURCE_DIR).
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb
  GIT_TAG f0569113c93ad095470c54bf34a17b36646bbbb5
)
FetchContent_MakeAvailable(stb)

# --- Ассет-вертикаль PoC (спека #5): assetc bake + рантайм-загрузка + шов в render ---
# zstd для bulk-кодека (decompress → арена). Desktop: brew/apt libzstd.
# zstd ПИННУТ из исходников (не системный) → байт-детерминизм бейка cross-machine
# (спека #5: pinned codec versions). Системный libzstd варьируется по версиям меж раннерами.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(zstd
  GIT_REPOSITORY https://github.com/facebook/zstd
  GIT_TAG v1.5.6
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(zstd)

# POSIX-only (fork/execvp bake-tools, mmap загрузка) — Windows-путь позже, как hotreload.
if(NOT WIN32)
  find_package(Threads REQUIRED)
  add_library(asset_core STATIC
    asset/bundle_writer.cpp asset/codec.cpp
    asset/platform_io.cpp asset/bundle_view.cpp asset/asset_manager.cpp)
  target_include_directories(asset_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/asset ${stb_SOURCE_DIR} ${zstd_SOURCE_DIR}/lib)
  target_link_libraries(asset_core PUBLIC libzstd_static Threads::Threads)

  add_executable(assetc asset/assetc_main.cpp)
  target_link_libraries(assetc PRIVATE asset_core)

  # Headless гейт #2 (zero-copy / no per-frame heap). ASan/UBSan-джоб — Phase 6.
  add_executable(asset_test asset/asset_test.cpp)
  target_link_libraries(asset_test PRIVATE asset_core)

  # Headless гейт #3 (замедленный I/O → тот же sim-hash). Без GPU — CI на всех POSIX-ОС.
  add_executable(asset_determinism_test asset/asset_determinism_test.cpp)
  target_link_libraries(asset_determinism_test PRIVATE asset_core)

  # Hot-reload roundtrip (свап бандла, стабильный guid). Self-contained — CI на всех POSIX.
  add_executable(asset_hot_reload_test asset/asset_hot_reload_test.cpp)
  target_link_libraries(asset_hot_reload_test PRIVATE asset_core)
endif()

# --- Render-вертикаль PoC (спека #2): deferred+forward, HDR/bloom, offscreen golden ---
add_library(render_core STATIC
  render/gpu.cpp render/arena.cpp render/scene.cpp render/sprite.cpp
  render/gpu_util.cpp render/shaders_gbuffer.cpp render/shaders_lighting.cpp
  render/shaders_post.cpp render/renderer.cpp render/renderer_pipelines.cpp
  render/renderer_forward.cpp render/renderer_bloom.cpp render/capture.cpp
  render/stb_impl.cpp)
target_include_directories(render_core PUBLIC ${stb_SOURCE_DIR})
target_link_libraries(render_core PUBLIC webgpu)

add_executable(render_demo render/demo_main.cpp)
target_link_libraries(render_demo PRIVATE render_core glfw webgpu glfw3webgpu)
target_copy_webgpu_binaries(render_demo)

# Headless T4-гейт: offscreen epsilon/perceptual golden (без окна, для CI).
add_executable(render_golden render/golden_main.cpp)
target_link_libraries(render_golden PRIVATE render_core webgpu)
target_copy_webgpu_binaries(render_golden)

# Автономный тест детерминизма (без GPU-зависимостей) — гоняется в CI на всех ОС/компиляторах.
add_executable(determinism_test src/determinism_test.cpp)

# Hot-reload PoC (dlopen/dlsym + sigsetjmp крэш-изоляция) — POSIX-only (dlfcn/sigaction),
# в Windows-джоб не входит. game_crash на -O0, чтобы null-deref не соптимизировался.
if(NOT WIN32)
    add_library(game_a MODULE hotreload/game_a.cpp)
    add_library(game_b MODULE hotreload/game_b.cpp)
    add_library(game_crash MODULE hotreload/game_crash.cpp)
    target_compile_options(game_crash PRIVATE -O0)
    set_target_properties(game_a game_b game_crash PROPERTIES PREFIX "")
    add_executable(hotreload_host hotreload/host.cpp)
endif()
