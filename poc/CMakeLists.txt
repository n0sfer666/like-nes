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

# --- Render-вертикаль PoC (спека #2): deferred+forward, HDR/bloom, offscreen golden ---
add_library(render_core STATIC
  render/gpu.cpp render/arena.cpp render/scene.cpp render/sprite.cpp
  render/gpu_util.cpp render/shaders_gbuffer.cpp render/shaders_lighting.cpp
  render/shaders_post.cpp render/renderer.cpp render/renderer_pipelines.cpp
  render/renderer_forward.cpp render/capture.cpp render/stb_impl.cpp)
target_include_directories(render_core PUBLIC ${stb_SOURCE_DIR})
target_link_libraries(render_core PUBLIC webgpu)

add_executable(render_demo render/demo_main.cpp)
target_link_libraries(render_demo PRIVATE render_core glfw webgpu glfw3webgpu)
target_copy_webgpu_binaries(render_demo)

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
