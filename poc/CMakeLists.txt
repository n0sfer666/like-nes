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

# --- basis_universal transcoder (KTX2/UASTC → BC7 в рантайме, спека #5) ---
# SOURCE_SUBDIR указывает в несуществующий подкаталог → MakeAvailable ТОЛЬКО выкачивает
# исходники (не билдит их CLI). Используем один файл транскодера. Пин по тегу (детерм.).
FetchContent_Declare(basisu
  GIT_REPOSITORY https://github.com/BinomialLLC/basis_universal
  GIT_TAG v1_60
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(basisu)

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

# --- miniaudio (AudioDevice HAL-бэкенд, спека #3) ---
# Single-header (как stb): SOURCE_SUBDIR в несуществующий подкаталог → MakeAvailable ТОЛЬКО
# выкачивает исходники. Реальное устройство — локально; в headless-CI выключается опцией.
option(AUDIO_MINIAUDIO "Real audio device via miniaudio (local; OFF в headless CI)" ON)
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio
  GIT_TAG 0.11.21
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(miniaudio)

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

# --- Аудио-вертикаль PoC (спека #3): mixer + шины + 2D-пан + стрим/резидент + детерминизм ---
# audio_core: микшер (Float/Fix32 seam) + декод (stb_vorbis, present в fetched stb). Без
# устройства/ассетов → deviceless-гейты гоняются в CI на всех POSIX.
if(NOT WIN32)
  add_library(audio_core STATIC
    audio/mixer.cpp audio/decoder.cpp audio/stb_vorbis_impl.c)
  target_include_directories(audio_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/audio ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/asset ${stb_SOURCE_DIR})
  target_link_libraries(audio_core PUBLIC Threads::Threads)

  # Гейт #1 (байт-golden хеш микса, Fix32 над детерм. PCM). CI на всех POSIX.
  add_executable(audio_golden audio/audio_golden_main.cpp)
  target_link_libraries(audio_golden PRIVATE audio_core)

  # Гейт #2 (замедл./джиттер callback → тот же sim-hash). TSan-джоб. CI на всех POSIX.
  add_executable(audio_determinism_test audio/audio_determinism_test.cpp)
  target_link_libraries(audio_determinism_test PRIVATE audio_core)

  # Гейт #3 (RT-safe callback: no-alloc/lock-free). ASan/UBSan-джоб. CI на всех POSIX.
  add_executable(audio_rt_test audio/audio_rt_test.cpp)
  target_link_libraries(audio_rt_test PRIVATE audio_core)

  # Шов asset->audio: бейкнутый vorbis → decode → mixer → offline WAV (+ --play на устройстве).
  add_executable(audio_seam audio/audio_seam_main.cpp)
  target_link_libraries(audio_seam PRIVATE audio_core asset_core)

  if(AUDIO_MINIAUDIO)
    add_library(audio_device STATIC audio/device.cpp audio/miniaudio_impl.c)
    target_include_directories(audio_device PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/audio ${CMAKE_CURRENT_SOURCE_DIR}/src ${miniaudio_SOURCE_DIR})
    target_compile_definitions(audio_device PUBLIC AUDIO_HAVE_MINIAUDIO)
    if(APPLE)
      target_link_libraries(audio_device PUBLIC "-framework CoreAudio" "-framework AudioToolbox"
        "-framework CoreFoundation")
    else()
      target_link_libraries(audio_device PUBLIC ${CMAKE_DL_LIBS} Threads::Threads m)
    endif()
    target_link_libraries(audio_seam PRIVATE audio_device)
  endif()
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

# --- Шов asset→render (спека #5 гейт): baked BC7 tex + baked Tint SPIR-V → GPU → offscreen ---
if(NOT WIN32)
  # basis транскодер — один файл; KTX2 без zstd-суперкомпрессии → BASISD_SUPPORT_KTX2_ZSTD=0
  # (не тянем вендорный zstd, нет конфликта символов с нашим libzstd).
  add_library(basis_transcoder STATIC ${basisu_SOURCE_DIR}/transcoder/basisu_transcoder.cpp)
  target_include_directories(basis_transcoder PUBLIC ${basisu_SOURCE_DIR}/transcoder)
  target_compile_definitions(basis_transcoder PUBLIC BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=0)

  add_library(asset_gpu STATIC asset/transcode.cpp)
  target_link_libraries(asset_gpu PUBLIC basis_transcoder)

  add_executable(asset_render asset/asset_render_main.cpp)
  target_include_directories(asset_render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/render)
  target_link_libraries(asset_render PRIVATE asset_core asset_gpu render_core webgpu)
  target_copy_webgpu_binaries(asset_render)
endif()

# --- input_core: action-map + coalesce + SPSC (спека #4). Deviceless-гейты — на всех ОС в CI.
# Native-бэкенды (XInput/RawInput · GameController · evdev) + live-демо — под INPUT_NATIVE ниже.
add_library(input_core STATIC input/action_map.cpp)
target_include_directories(input_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/input ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/asset)
target_link_libraries(input_core PUBLIC Threads::Threads)

add_executable(input_determinism_test input/input_determinism_test.cpp)
target_link_libraries(input_determinism_test PRIVATE input_core)

add_executable(input_race_test input/input_race_test.cpp)
target_link_libraries(input_race_test PRIVATE input_core)

add_executable(input_rt_test input/input_rt_test.cpp)
target_link_libraries(input_rt_test PRIVATE input_core)

add_executable(input_actionmap_test input/input_actionmap_test.cpp)
target_link_libraries(input_actionmap_test PRIVATE input_core)

# Live-демо: GLFW-пумп (kbd/mouse) + native gamepad (GameController/XInput/evdev) + rumble.
# Реальные устройства → owner-HW (macOS live). OFF в headless CI (нужен дисплей/HID).
option(INPUT_NATIVE "Build native input demo (GLFW + platform gamepad backend)" ON)
if(INPUT_NATIVE)
  if(APPLE)
    set(INPUT_GAMEPAD_SRC input/source_gamepad_macos.mm)
  elseif(WIN32)
    set(INPUT_GAMEPAD_SRC input/source_gamepad_win.cpp)
  else()
    set(INPUT_GAMEPAD_SRC input/source_gamepad_linux.cpp)
  endif()
  add_executable(input_demo input/input_demo_main.cpp input/source_glfw.cpp ${INPUT_GAMEPAD_SRC})
  target_link_libraries(input_demo PRIVATE input_core glfw)
  if(APPLE)
    # ARC для .mm: без него createEngine/createPlayer (autoreleased) повисают → UAF + утечки.
    set_source_files_properties(input/source_gamepad_macos.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(input_demo PRIVATE "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
  elseif(WIN32)
    target_link_libraries(input_demo PRIVATE Xinput9_1_0)
  endif()
endif()

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

# --- plugin_core: ExtensionPointRegistry + PluginHost (C-ABI native + WASM-точка) + встроенные
# sim-системы (спека #6). Загрузчик кросс-платформенный (dlopen/LoadLibrary); сигнал-изоляция —
# POSIX (Windows SEH — follow-up). Гейты determinism/seam — на всех 3 OS; isolation — POSIX.
add_library(plugin_core STATIC plugin/registry.cpp plugin/host.cpp plugin/builtin.cpp plugin/manifest.cpp)
target_include_directories(plugin_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/plugin ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(NOT WIN32)
  target_link_libraries(plugin_core PUBLIC ${CMAKE_DL_LIBS})
endif()

add_executable(plugin_determinism_test plugin/plugin_determinism_test.cpp)
target_link_libraries(plugin_determinism_test PRIVATE plugin_core)

add_executable(plugin_seam_test plugin/plugin_seam_test.cpp)
target_link_libraries(plugin_seam_test PRIVATE plugin_core)

add_executable(plugin_isolation_test plugin/plugin_isolation_test.cpp)
target_link_libraries(plugin_isolation_test PRIVATE plugin_core)

# Headless-гейт конфигурируемого интерфейса: манифест (данные) → панели/докинг. Без окна, в CI.
add_executable(plugin_manifest_test plugin/plugin_manifest_test.cpp)
target_link_libraries(plugin_manifest_test PRIVATE plugin_core)

# Native C-ABI плагины как MODULE (.so/.dll без lib-префикса). crash на -O0, чтобы null-deref
# не соптимизировался (как game_crash в hot-reload).
add_library(plugin_gravity MODULE plugin/plugins/gravity.cpp)
add_library(plugin_wind MODULE plugin/plugins/wind.cpp)
add_library(plugin_rle_codec MODULE plugin/plugins/rle_codec.cpp)
add_library(plugin_multi MODULE plugin/plugins/multi.cpp)
add_library(plugin_crash MODULE plugin/plugins/crash.cpp)
target_compile_options(plugin_crash PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O0>)
foreach(P plugin_gravity plugin_wind plugin_rle_codec plugin_multi plugin_crash)
  target_include_directories(${P} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin ${CMAKE_CURRENT_SOURCE_DIR}/src)
  set_target_properties(${P} PROPERTIES PREFIX "")
endforeach()

# Live UI-shell: Dear ImGui (docking) + GLFW + OpenGL3. Докируемые панели из манифестов.
# Owner-HW (macOS live). OFF в headless CI (нужен дисплей/GL).
option(PLUGIN_UI "Build live plugin UI-shell (Dear ImGui docking + GLFW)" ON)
if(PLUGIN_UI)
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5-docking)
  FetchContent_MakeAvailable(imgui)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui PUBLIC glfw)
  add_executable(plugin_ui_shell plugin/ui_shell_main.cpp)
  target_link_libraries(plugin_ui_shell PRIVATE plugin_core imgui glfw)
  if(APPLE)
    target_link_libraries(plugin_ui_shell PRIVATE "-framework OpenGL")
  elseif(WIN32)
    target_link_libraries(plugin_ui_shell PRIVATE opengl32)
  else()
    target_link_libraries(plugin_ui_shell PRIVATE GL)
  endif()
endif()

# WASM-sandbox гейт (спека #6, gate #5): untrusted-плагин в wasmtime-песочнице. Guest — WAT
# (i32 fix32, детерм.), грузится через wasmtime_wat2wasm (без wasm-ld). Local pinned-T4 (C-API
# prebuilt в poc/deps, не в git; CI — follow-up). Гейты: native≡WASM sim-hash + OOB/cap escape→trap.
option(PLUGIN_WASM "Build WASM-sandbox gate (wasmtime C-API from poc/deps)" ON)
if(PLUGIN_WASM)
  file(GLOB WASMTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/wasmtime-*-c-api)
  if(WASMTIME_DIR AND EXISTS ${WASMTIME_DIR}/include/wasmtime.h)
    add_executable(plugin_wasm_test plugin/plugin_wasm_test.cpp plugin/wasm_host.cpp)
    target_include_directories(plugin_wasm_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/plugin ${CMAKE_CURRENT_SOURCE_DIR}/src ${WASMTIME_DIR}/include)
    target_link_libraries(plugin_wasm_test PRIVATE plugin_core ${WASMTIME_DIR}/lib/libwasmtime.a)
    if(APPLE)
      target_link_libraries(plugin_wasm_test PRIVATE
        "-framework CoreFoundation" "-framework Security" "-framework SystemConfiguration")
    elseif(UNIX)
      target_link_libraries(plugin_wasm_test PRIVATE pthread dl m)
    endif()
  else()
    message(STATUS "PLUGIN_WASM: wasmtime C-API не найден в poc/deps — gate #5 пропущен")
  endif()
endif()

# --- IDE-вертикаль (спека #7) — фундамент: flecs scene-doc + детерм. round-trip + undo/redo ---
# flecs = ECS-мир + meta/json рефлексия (единый источник: property-grid + IPC-зеркало + save/load).
# Гейт 1 (scene round-trip): текст-source(JSON)+стабильные GUID → save->reload->save байт-идентичны +
# детерм. golden bake-hash (fix32 целочисл. → cross-arch). Гейт 2 (undo/redo): command-bus. На 3 OS.
option(IDE_POC "Build IDE editor foundation (flecs scene-doc + round-trip + undo)" ON)
if(IDE_POC)
  set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
  set(FLECS_STATIC ON CACHE BOOL "" FORCE)
  set(FLECS_PIC ON CACHE BOOL "" FORCE)
  set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs
    GIT_TAG v4.1.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(flecs)

  add_library(scene_core STATIC ide/scene.cpp ide/serialize.cpp)
  target_include_directories(scene_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ide ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(scene_core PUBLIC flecs_static)

  add_executable(scene_roundtrip_test ide/scene_roundtrip_test.cpp)
  target_link_libraries(scene_roundtrip_test PRIVATE scene_core)

  add_executable(command_test ide/command_test.cpp)
  target_link_libraries(command_test PRIVATE scene_core)
endif()
