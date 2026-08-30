# backend_host.cpp собирается прямо сюда: порядок разрушения backend→dlclose иначе не покрыт
# ничем, а под ASan-гейтом (-DPLUGIN_FORCE_DLCLOSE) это единственный способ его проверить.
set(ACH_TEST_DIR ${CMAKE_SOURCE_DIR}/engine/achievements)
add_executable(ach_plugin_test
  ${ACH_TEST_DIR}/plugin_test.cpp ${ACH_TEST_DIR}/plugin_test_common.cpp
  ${ACH_TEST_DIR}/plugin_host_test.cpp backend_host.cpp)
target_link_libraries(ach_plugin_test PRIVATE ach_plugin)

# --- Образец-платформер, машинная половина (шаг C вертикали 3 спеки #16): скриптованный прогон по
# уровню из game.bundle → голден траектории. Стоит ДО выхода по IDE_POC вместе с `ach_plugin_test`,
# а не рядом с остальными sim-целями ниже: ни окна, ни WebGPU, ни flecs ему не нужно, и цель,
# спрятанная за чужой опцией, молча выпадала бы из headless-набора, которым её и гоняют.
add_executable(game_platformer_sim_test
  platformer_sim_test.cpp platformer_sim.cpp platformer_scene.cpp)
target_include_directories(game_platformer_sim_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_sim_test PRIVATE framework_character framework_tilemap
  asset_core platform_core)

# Чистая половина ЖИВОЙ цели — камера и сборка квадов (шаг C вертикали 3). Тоже до выхода по
# IDE_POC и по тому же основанию: рисовать ей нечем, ни окна, ни адаптера она не открывает, и
# спрятанная за чужой опцией она проверялась бы только там, где собирают всё дерево, — то есть на
# машине владельца, где живое окно и так открывается глазами.
add_executable(game_platformer_view_test
  platformer_view_test.cpp platformer_view.cpp platformer_scene.cpp)
# framework_graphics_tiles СТОИТ ВМЕСТО framework_tilemap и platform_core, а не рядом с ними: карту
# он тянет PUBLIC-связью, платформу — PRIVATE-связью своего framework_graphics, и названные сверх
# этого вторым именем они попадают на линкер-строку дважды — macOS отвечает `warning: ignoring
# duplicate libraries`, мимо -Werror, но прямо в лог гейта. Библиотека приезжает транзитивно, а вот
# заголовки PRIVATE-связь не несёт — отсюда каталог платформы, названный руками.
target_include_directories(game_platformer_view_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/engine/platform)
target_link_libraries(game_platformer_view_test PRIVATE framework_character
  framework_graphics_tiles asset_core)

# Шов ввода образца — раскладка в намерение персонажа. Тоже headless: манифест печётся `preset_bake`
# из движка, а кадр ввода собирается руками, поэтому ни окна, ни ОС цели не нужно.
# Анти-дрейф нарезки (спека #17, шаг D): у игры-образца ДВА источника регионов — `assets/atlas.txt`
# в бандле и `game::set_regions()` в коде, — и живут они оба, пока рендер не переехал на слой
# (вертикаль 3). Гейт сверяет их числами, снимая полтексела `game::rgn`. Цель headless: `art.cpp` —
# чистый CPU, ни окна, ни RHI ему не нужно.
add_executable(game_atlas_regions_test game_atlas_regions_test.cpp art.cpp)
target_include_directories(game_atlas_regions_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_atlas_regions_test PRIVATE framework_graphics asset_core platform_core)

add_executable(game_platformer_input_test platformer_input_test.cpp platformer_input.cpp)
# platform_core тут НЕ в списке библиотек, хотя `platform::Args` цели нужен: и framework_input, и
# framework_character держат его PRIVATE, то есть каждый дописывает его в замыкание СВОЕЙ строкой,
# и названный сверх этого явно он попадает на линкер-строку дважды — ld на macOS отвечает
# `warning: ignoring duplicate libraries`. Предупреждение идёт мимо -Werror, но прямо в лог,
# который читает build_check.sh, поэтому гейт от него красный. Тот же случай расписан у
# game_sidescroller ниже. Библиотека приезжает транзитивно, а вот заголовки PRIVATE-связь не
# несёт — отсюда каталог, названный руками.
target_include_directories(game_platformer_input_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/engine/platform)
target_link_libraries(game_platformer_input_test PRIVATE framework_input framework_character)

if(NOT IDE_POC)
  return()
endif()

# --- Скелет игры: flecs+fix32 fixed-timestep + ввод #4 + WebGPU спрайт-батч (инстансинг) +
# плейсхолдер-спрайты. GpuContext переиспользуется из engine/render/gpu.cpp (без всего render_core);
# батч — свой. --demo пишет offscreen PNG-кадры (headless, для GIF/CI-smoke).
set(GAME_INPUT_SRC ${CMAKE_SOURCE_DIR}/engine/input/source_glfw.cpp)
if(APPLE)
  list(APPEND GAME_INPUT_SRC ${CMAKE_SOURCE_DIR}/engine/input/source_gamepad_macos.mm)
elseif(WIN32)
  list(APPEND GAME_INPUT_SRC ${CMAKE_SOURCE_DIR}/engine/input/source_gamepad_win.cpp)
else()
  list(APPEND GAME_INPUT_SRC ${CMAKE_SOURCE_DIR}/engine/input/source_gamepad_linux.cpp)
endif()
# Шов assetc→билд (спека #8, гейт 2): baked atlas из game.bundle (asset_core + asset_gpu
# транскод UASTC→BC7). Заглушки под Windows сняты вместе с POSIX-only'ем asset_core (спека #12).
set(GAME_SEAM_SRC assets_path.cpp bundle_atlas.cpp ach_source_bundle.cpp)
add_executable(game_sidescroller
  main.cpp live.cpp demo.cpp draw.cpp
  achievements.cpp backend_host.cpp
  batch.cpp art.cpp sim.cpp combat.cpp boss.cpp fx.cpp bloom.cpp audio.cpp input_setup.cpp capture.cpp
  gpu_env.cpp
  ${GAME_SEAM_SRC} ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp ${GAME_INPUT_SRC})
target_include_directories(game_sidescroller PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/render)
target_include_directories(game_sidescroller SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(game_sidescroller PRIVATE
  engine_core framework_input flecs_static webgpu glfw3webgpu glfw ach_plugin
  Threads::Threads)
# asset_core (и через него platform_core) приходит транзитивно из ach_bundle. Явным его тут не
# держим: CMake дедуплицирует ПРЯМОЙ список, но транзитивное замыкание дописывает ПОСЛЕ него, и
# библиотека, названная обоими способами, попадает на линкер-строку дважды — ld на macOS отвечает
# `warning: ignoring duplicate libraries`. Проверено на build.ninja: libasset_core.a стояла и в
# начале строки, и в хвосте замыкания.
target_link_libraries(game_sidescroller PRIVATE asset_gpu ach_bundle)
# Аудио #3 (локально): mixer + miniaudio-устройство. AUDIO_HAVE_MINIAUDIO — из audio_device
# (PUBLIC) → audio.cpp компилит реальный путь; без AUDIO_MINIAUDIO — no-op (CI headless).
if(AUDIO_MINIAUDIO)
  target_link_libraries(game_sidescroller PRIVATE audio_core audio_device)
endif()
set_source_files_properties(capture.cpp PROPERTIES   # stb_image_write sprintf (не MSVC:
  COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-deprecated-declarations>")  # D8021)
if(APPLE)
  set_source_files_properties(${CMAKE_SOURCE_DIR}/engine/input/source_gamepad_macos.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(game_sidescroller PRIVATE
    "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
elseif(WIN32)
  target_link_libraries(game_sidescroller PRIVATE Xinput9_1_0)
endif()
target_copy_webgpu_binaries(game_sidescroller)
reproducible_rpath(game_sidescroller)

# --- Образец-платформер, ЖИВАЯ половина (гейт 8 спеки #16): окно, часы и ввод поверх той же сцены,
# которую гоняет sim-голден. Отдельная цель, а не режим `game_sidescroller`: у шутера нет ни
# гравитации, ни тайлов, и общего у них ровно два куска — спрайт-батч и шов ввода.
#
# flecs приезжает сюда транзитивно и не по нужде: `batch.cpp` включает `world.hpp` ради двух
# констант вида. Платформер их перекрывает своим `set_viewport`, но цель всё равно обязана
# слинковаться, поэтому библиотека названа явно, а не выпилена из чужого файла.
add_executable(game_platformer
  platformer_live.cpp platformer_view.cpp platformer_input.cpp platformer_scene.cpp
  batch.cpp art.cpp gpu_env.cpp assets_path.cpp input_setup.cpp
  ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp ${GAME_INPUT_SRC})
# platform_core здесь не назван по тому же основанию, что и в гейте выше: framework_graphics_tiles
# тянет его PRIVATE-связью своего framework_graphics, и второе имя дало бы `warning: ignoring
# duplicate libraries` на macOS — мимо -Werror, но прямо в лог гейта. Заголовки платформы такая
# связь не несёт, поэтому её каталог назван руками.
target_include_directories(game_platformer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/render ${CMAKE_SOURCE_DIR}/engine/platform)
target_link_libraries(game_platformer PRIVATE
  engine_core framework_character framework_graphics_tiles framework_input asset_core
  flecs_static webgpu glfw3webgpu glfw Threads::Threads)
if(APPLE)
  target_link_libraries(game_platformer PRIVATE
    "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
elseif(WIN32)
  target_link_libraries(game_platformer PRIVATE Xinput9_1_0)
endif()
target_copy_webgpu_binaries(game_platformer)
reproducible_rpath(game_platformer)

# --- Packaging per-OS (спека #8, гейт 4): самодостаточный бандл + version-stamp (git-hash).
# macOS .app (@rpath→Frameworks) / Linux tarball ($ORIGIN, flat) / Windows папка (.exe+DLL).
# game.bundle (baked ассеты, гейт 2) кладётся в бандл → игра стартует с бейкнутых, не с source.
# Сборка бандла: cmake --install <build> --prefix <out> (см. scripts/package.sh).
execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
  OUTPUT_VARIABLE GAME_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GAME_GIT_HASH)
  set(GAME_GIT_HASH "unknown")
endif()
# Версия из тега (release.yml: -DGAME_VERSION=vX.Y.Z), иначе dev-дефолт.
if(NOT GAME_VERSION)
  set(GAME_VERSION "0.1.0-dev")
endif()
set(GAME_TARGET_TRIPLE "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
configure_file(packaging/version.txt.in ${CMAKE_BINARY_DIR}/version.txt @ONLY)
set(GAME_BUNDLE_ASSET ${CMAKE_CURRENT_SOURCE_DIR}/assets/game.bundle)

include(${CMAKE_SOURCE_DIR}/cmake/licenses.cmake)

if(APPLE)
  set_target_properties(game_sidescroller PROPERTIES
    INSTALL_RPATH "@executable_path;@executable_path/../Frameworks")
  configure_file(packaging/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)
  set(_app "like-nes.app/Contents")
  install(TARGETS game_sidescroller RUNTIME DESTINATION ${_app}/MacOS COMPONENT game)
  install(FILES ${WGPU_RUNTIME_LIB} DESTINATION ${_app}/Frameworks COMPONENT game)
  install(FILES ${GAME_BUNDLE_ASSET} ${CMAKE_BINARY_DIR}/version.txt
          DESTINATION ${_app}/Resources COMPONENT game)
  install(FILES ${LICENSE_FILES} DESTINATION ${_app}/Resources/licenses COMPONENT game)
  install(FILES ${CMAKE_BINARY_DIR}/Info.plist DESTINATION ${_app} COMPONENT game)
else()
  install(TARGETS game_sidescroller RUNTIME DESTINATION like-nes COMPONENT game)
  install(FILES ${WGPU_RUNTIME_LIB} ${GAME_BUNDLE_ASSET} ${CMAKE_BINARY_DIR}/version.txt
          DESTINATION like-nes COMPONENT game)
  install(FILES ${LICENSE_FILES} DESTINATION like-nes/licenses COMPONENT game)
endif()

# --- Sim-hash регресс-тест боя (S7, гейт): headless (без рендера) → собирается/гоняется на
# всех ОС. Скриптованный combat-сценарий → детерм. golden-хеш (fix32/целочисл. → cross-arch).
add_executable(game_sim_test sim_test.cpp sim.cpp combat.cpp boss.cpp)
target_include_directories(game_sim_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/input)
target_link_libraries(game_sim_test PRIVATE engine_core flecs_static Threads::Threads)

# Гейт 7 спеки #10: наблюдатель поверх скриптованного прогона — sim-хеш обязан остаться golden'ом #8.
add_executable(ach_sim_test
  ach_sim_test.cpp achievements.cpp backend_host.cpp ach_source_bundle.cpp
  sim.cpp combat.cpp boss.cpp)
target_include_directories(ach_sim_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/input)
target_link_libraries(ach_sim_test PRIVATE engine_core ach_bundle ach_plugin flecs_static Threads::Threads)
