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
  platformer_sim_test.cpp platformer_sim.cpp platformer_scene.cpp platformer_level.cpp)
target_include_directories(game_platformer_sim_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_sim_test PRIVATE framework_character framework_tilemap
  asset_core platform_core)

# Тот же уровень и тот же маршрут, но под откатом (спека #22, вертикаль 1, шаг A3). Отдельной целью
# от прогона выше, потому что предмет другой: тот утверждает, что маршрут воспроизводим на трёх ОС,
# этот — что он воспроизводим ПОСЛЕ ОТКАТА, то есть что снимок сцены ничего не теряет.
add_executable(game_platformer_rollback_test
  platformer_rollback_test.cpp platformer_sim.cpp platformer_scene.cpp platformer_level.cpp)
target_include_directories(game_platformer_rollback_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_rollback_test PRIVATE framework_character framework_tilemap
  framework_rollback framework_alloc_probe_control asset_core platform_core)

# Гейт 6 спеки #22: записанный прогон переигрывается заново. Целей ДВЕ по числу вопросов — остра
# ли заявка о состоянии и тотален ли читатель формата, — и обстановка у них общая
# (`platformer_replay_run.hpp`): второй экземпляр записи был бы второй правдой о том, что такое
# «тот же прогон».
foreach(T game_platformer_replay_test game_platformer_replay_io_test)
  add_executable(${T} ${T}.cpp platformer_sim.cpp platformer_scene.cpp platformer_level.cpp)
  target_include_directories(${T} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${T} PRIVATE framework_character framework_tilemap framework_rollback
    framework_replay framework_alloc_probe_control net_core asset_core platform_core)
endforeach()

# Гейты 1 и 7 спеки #22: два ПРОЦЕССА над той же сценой, канал между ними — настоящий сокет.
# Отдельной целью от гейта отката выше, потому что предмет другой: тот утверждает, что снимок сцены
# ничего не теряет, этот — что из одного ввода два адресных пространства приходят в одно состояние.
# Двоичный файл один на обе роли: `--peer send|recv` перезапускает СОБСТВЕННЫЙ exe, потому что
# fork'а на Windows нет (`platform_process.hpp`).
add_executable(game_platformer_net_test
  platformer_net_test.cpp platformer_peer.cpp platformer_sim.cpp platformer_scene.cpp
  platformer_level.cpp)
target_include_directories(game_platformer_net_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_net_test PRIVATE framework_character framework_tilemap
  framework_rollback framework_replay framework_alloc_probe_control net_core asset_core platform_core
  Threads::Threads)

# Чистая половина ЖИВОЙ цели — камера и сборка квадов (шаг C вертикали 3). Тоже до выхода по
# IDE_POC и по тому же основанию: рисовать ей нечем, ни окна, ни адаптера она не открывает, и
# спрятанная за чужой опцией она проверялась бы только там, где собирают всё дерево, — то есть на
# машине владельца, где живое окно и так открывается глазами.
add_executable(game_platformer_view_test
  platformer_view_test.cpp platformer_view.cpp platformer_scene.cpp platformer_level.cpp)
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
# Анти-дрейф нарезки (спека #17, шаги D и B вертикали 3): у игры-образца ДВА источника регионов —
# секция `atlas_regions` бандла (бейкнутый путь) и `game::set_regions()` (процедурный fallback,
# рисующий пиксели сам). Гейт сверяет их числами, снимая полтексела `game::rgn`, и отдельно
# утверждает, что бейкнутый путь читает ТАБЛИЦУ. Цель headless: `art.cpp` и `atlas_regions.cpp` —
# чистый CPU, ни окна, ни RHI им не нужно (`bundle_atlas.cpp` сюда не входит: он тянет транскод).
add_executable(game_atlas_regions_test game_atlas_regions_test.cpp art.cpp atlas_regions.cpp)
target_include_directories(game_atlas_regions_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_atlas_regions_test PRIVATE framework_graphics asset_core platform_core)

# Гейт 5 спеки #18 на стороне игры-образца: нарезка кадра на прогоны одного материала. Цель
# headless и стоит здесь, ДО выхода по IDE_POC, по тому же основанию, что и соседи: ни окна, ни
# WebGPU нарезке не нужно, а цель, спрятанная за чужой опцией, молча выпадала бы из набора,
# которым её и гоняют.
add_executable(game_material_runs_test game_material_runs_test.cpp material_runs.cpp)
target_include_directories(game_material_runs_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
  batch.cpp sprite_pipeline.cpp material_fx.cpp material_runs.cpp scene_fx.cpp
  instance_stage.cpp art.cpp atlas_regions.cpp sprite_out.cpp sim.cpp combat.cpp
  boss.cpp fx.cpp
  bloom.cpp audio.cpp
  input_setup.cpp capture.cpp frame_golden.cpp
  gpu_env.cpp
  ${GAME_SEAM_SRC} ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp ${GAME_INPUT_SRC})
target_include_directories(game_sidescroller PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/render
  ${CMAKE_SOURCE_DIR}/engine/framework)
target_include_directories(game_sidescroller SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(game_sidescroller PRIVATE
  engine_core framework_input framework_graphics flecs_static webgpu glfw3webgpu glfw ach_plugin
  Threads::Threads)
# asset_core (и через него platform_core) приходит транзитивно из ach_bundle. Явным его тут не
# держим: CMake дедуплицирует ПРЯМОЙ список, но транзитивное замыкание дописывает ПОСЛЕ него, и
# библиотека, названная обоими способами, попадает на линкер-строку дважды — ld на macOS отвечает
# `warning: ignoring duplicate libraries`. Проверено на build.ninja: libasset_core.a стояла и в
# начале строки, и в хвосте замыкания.
target_link_libraries(game_sidescroller PRIVATE asset_gpu ach_bundle)
# Библиотека эффектов (спека #18) + драйвер горячей замены (гейт 3): правка шейдера проверяется в
# том, что рисует. Названа только `material_hot`: `material_gpu`, `material_core` и `platform_core`
# он тянет PUBLIC-связью, и второе имя дало бы `warning: ignoring duplicate libraries` на macOS.
target_link_libraries(game_sidescroller PRIVATE material_hot)
# Реализация stb (чтение PNG для гейта 2 и запись кадров) приезжает ТОЙ ЖЕ вендоренной OBJECT-целью,
# что и у render-вертикали, а не вторым `#define … _IMPLEMENTATION` в нашем capture.cpp: уровень
# предупреждений — свойство цели, и снятый с одного исходника через set_source_files_properties он
# давал MSVC `D8021`/`D9025` мимо -Werror, но прямо в лог гейта (cmake/warnings.cmake).
target_link_libraries(game_sidescroller PRIVATE stb_impl_vendored)
# Аудио #3 (локально): mixer + miniaudio-устройство. AUDIO_HAVE_MINIAUDIO — из audio_device
# (PUBLIC) → audio.cpp компилит реальный путь; без AUDIO_MINIAUDIO — no-op (CI headless).
if(AUDIO_MINIAUDIO)
  target_link_libraries(game_sidescroller PRIVATE audio_core audio_device)
endif()
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

# Шов «спрайт фреймворка → инстанс шутера» (вертикаль 3, шаг B3). Цель headless: `Instance` живёт
# отдельным заголовком от `batch.hpp` именно затем, чтобы гейт, считающий ЧТО уезжает на видеокарту,
# не поднимал ради этого WebGPU.
add_executable(game_sprite_out_test game_sprite_out_test.cpp sprite_out.cpp instance_stage.cpp
  art.cpp atlas_regions.cpp)
target_include_directories(game_sprite_out_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/framework)
# material_core — из-за `game::Instance = mat::Instance` (спека #18): один вершинный этап на оба
# потребителя библиотеки. GPU сюда не приезжает, цель как была headless, так и осталась.
target_link_libraries(game_sprite_out_test PRIVATE framework_graphics
  framework_alloc_probe_control asset_core platform_core material_core)

# Частицы шутера на фреймворке (вертикаль 3, шаг B3): голден сцены + гейт 8 + симметрия звезды.
# Отдельно от `game_sprite_out_test`, а не случаем внутри него, по двум причинам сразу.
# `framework_alloc_probe.hpp` подменяет глобальные operator new/delete и допустим ровно в ОДНОЙ цели
# на программу — тот же довод, по которому у батча фреймворка гейт 8 отделён от гейта сборки.
# И предмет разный: адаптер проверяется единицами и отказами, частицы — числом за прогон, и слитый
# гейт называл бы в логе CI не тот слой. flecs приезжает ради обхода следов в `emit_trails`,
# WebGPU — нет: `Instance` живёт отдельным заголовком именно затем.
add_executable(game_fx_test game_fx_test.cpp fx.cpp sprite_out.cpp art.cpp atlas_regions.cpp)
target_include_directories(game_fx_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/framework)
# platform_core тут НЕ назван, хотя нужен: framework_graphics и framework_input дописывают его в
# замыкание каждый своей PRIVATE-связью, и названный сверх этого явно он попадает на линкер-строку
# дважды — ld на macOS отвечает `warning: ignoring duplicate libraries`, мимо -Werror, но прямо в
# лог гейта. Тот же случай расписан у game_platformer_input_test выше.
target_link_libraries(game_fx_test PRIVATE framework_graphics framework_input
  framework_alloc_probe_control flecs_static asset_core material_core)

# --- Симметрия звезды атласа: своя цель, а не третий тест в `game_fx_test`. Она стоит на ПЕЧЁНОМ
# атласе и не трогает ни `Fx`, ни flecs — в общем файле гейт краснел бы на правке траектории.
add_executable(game_star_test game_star_test.cpp art.cpp atlas_regions.cpp)
target_include_directories(game_star_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/framework)
target_link_libraries(game_star_test PRIVATE framework_graphics asset_core)

# --- Образец-платформер, ЖИВАЯ половина (гейт 8 спеки #16): окно, часы и ввод поверх той же сцены,
# которую гоняет sim-голден. Отдельная цель, а не режим `game_sidescroller`: у шутера нет ни
# гравитации, ни тайлов, и общего у них ровно два куска — спрайт-батч и шов ввода.
#
# flecs приезжает сюда транзитивно и не по нужде: `batch.cpp` включает `world.hpp` ради двух
# констант вида. Платформер их перекрывает своим `set_viewport`, но цель всё равно обязана
# слинковаться, поэтому библиотека названа явно, а не выпилена из чужого файла.
add_executable(game_platformer
  platformer_live.cpp platformer_view.cpp platformer_input.cpp platformer_scene.cpp platformer_level.cpp
  batch.cpp sprite_pipeline.cpp material_fx.cpp material_runs.cpp
  instance_stage.cpp art.cpp gpu_env.cpp assets_path.cpp input_setup.cpp
  ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp ${GAME_INPUT_SRC})
# platform_core здесь не назван по тому же основанию, что и в гейте выше: framework_graphics_tiles
# тянет его PRIVATE-связью своего framework_graphics, и второе имя дало бы `warning: ignoring
# duplicate libraries` на macOS — мимо -Werror, но прямо в лог гейта. Заголовки платформы такая
# связь не несёт, поэтому её каталог назван руками.
target_include_directories(game_platformer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/render ${CMAKE_SOURCE_DIR}/engine/platform)
target_link_libraries(game_platformer PRIVATE
  engine_core framework_character framework_graphics_tiles framework_input asset_core
  material_hot flecs_static webgpu glfw3webgpu glfw Threads::Threads)
if(APPLE)
  target_link_libraries(game_platformer PRIVATE
    "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
elseif(WIN32)
  target_link_libraries(game_platformer PRIVATE Xinput9_1_0)
endif()
target_copy_webgpu_binaries(game_platformer)
reproducible_rpath(game_platformer)

include(${CMAKE_CURRENT_SOURCE_DIR}/packaging.cmake)

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
