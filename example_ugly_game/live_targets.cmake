# Оконные цели образца-платформера: одиночный прогон (гейт 8 спеки #16) и сетевой пир (гейт 9 спеки
# #22). Своим файлом, а не строками в CMakeLists.txt рядом: тот упёрся в предел длины ровно на
# второй из них, и запись бюджета прямо требовала разреза по границе смысла. Граница есть: это
# единственные цели каталога, которым нужны окно GLFW, устройство wgpu и живой ввод.
#
# flecs приезжает сюда транзитивно и не по нужде: `batch.cpp` включает `world.hpp` ради двух
# констант вида. Платформер их перекрывает своим `set_viewport`, но цель всё равно обязана
# слинковаться, поэтому библиотека названа явно, а не выпилена из чужого файла.

# Список общий, потому что обе цели показывают ОДНУ сцену одним батчем. Разойдись копии — одна из
# целей рисовала бы вчерашним кодом, и увидел бы это только человек за экраном.
set(PLATFORMER_LIVE_SRC
  platformer_window.cpp platformer_live_input.cpp
  platformer_view.cpp platformer_input.cpp platformer_scene.cpp platformer_level.cpp
  batch.cpp sprite_pipeline.cpp material_fx.cpp material_runs.cpp
  instance_stage.cpp art.cpp gpu_env.cpp assets_path.cpp input_setup.cpp
  ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp ${GAME_INPUT_SRC})

# platform_core здесь не назван по тому же основанию, что и в гейтах выше: framework_graphics_tiles
# тянет его PRIVATE-связью своего framework_graphics, и второе имя дало бы `warning: ignoring
# duplicate libraries` на macOS — мимо -Werror, но прямо в лог гейта. Заголовки платформы такая
# связь не несёт, поэтому её каталог назван руками.
function(platformer_live_target name)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/render ${CMAKE_SOURCE_DIR}/engine/platform)
  target_link_libraries(${name} PRIVATE
    engine_core framework_character framework_graphics_tiles framework_input asset_core
    material_hot flecs_static webgpu glfw3webgpu glfw Threads::Threads)
  if(APPLE)
    target_link_libraries(${name} PRIVATE
      "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
  elseif(WIN32)
    target_link_libraries(${name} PRIVATE Xinput9_1_0)
  endif()
  target_copy_webgpu_binaries(${name})
  reproducible_rpath(${name})
endfunction()

# --- Образец-платформер, ЖИВАЯ половина (гейт 8 спеки #16): окно, часы и ввод поверх той же сцены,
# которую гоняет sim-голден. Отдельная цель, а не режим `game_sidescroller`: у шутера нет ни
# гравитации, ни тайлов, и общего у них ровно два куска — спрайт-батч и шов ввода.
add_executable(game_platformer platformer_live.cpp ${PLATFORMER_LIVE_SRC})
platformer_live_target(game_platformer)

# --- Живая половина ПИРА (гейт 9 спеки #22): то же окно и тот же ввод, но такт даёт сеть, а не
# цикл цели. Отдельным исполняемым от `game_platformer`, а не флагом: одиночному прогону сокет не
# нужен вовсе, и цель, которая его умеет, тащила бы net_core в игру, где сети нет.
add_executable(game_platformer_net_live
  platformer_net_live.cpp platformer_live_hooks.cpp platformer_peer.cpp platformer_sim.cpp
  ${PLATFORMER_LIVE_SRC})
platformer_live_target(game_platformer_net_live)
target_link_libraries(game_platformer_net_live PRIVATE
  framework_rollback framework_replay framework_alloc_probe_control net_core)
