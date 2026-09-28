# Игра-образец без окна: сцена, симуляция, материалы, звук, достижения, пресеты ввода. Один список на
# десктоп (`game_sidescroller`) и обе мобильные оболочки (platform/ios, platform/android), потому что
# копия списка в оболочке уже однажды застыла: удалённый `controls.cpp` оставался в ней, пока
# оболочки не перестали собираться вовсе, и узнать это было нечем.
#
# Своё у каждой стороны — только вход и кадр: GLFW и геймпад на десктопе, касания и слой Metal /
# ANativeWindow на мобиле. Пути абсолютные: файл включают из чужих каталогов.
set(GAME_CORE_SRC
  draw.cpp achievements.cpp backend_host.cpp
  batch.cpp sprite_pipeline.cpp material_fx.cpp material_runs.cpp scene_fx.cpp
  instance_stage.cpp art.cpp atlas_regions.cpp sprite_out.cpp sim.cpp combat.cpp
  boss.cpp fx.cpp bloom.cpp audio.cpp input_setup.cpp gpu_env.cpp
  assets_path.cpp bundle_atlas.cpp ach_source_bundle.cpp)
list(TRANSFORM GAME_CORE_SRC PREPEND ${CMAKE_CURRENT_LIST_DIR}/)
list(APPEND GAME_CORE_SRC
  ${CMAKE_SOURCE_DIR}/engine/render/surface_frame.cpp ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
set(GAME_CORE_DIR ${CMAKE_CURRENT_LIST_DIR})

# Цель материалов — параметром: десктоп берёт `material_hot` (горячая замена `.wgsl`), мобиле
# хватает `material_gpu`. Назвать обе нельзя: hot тянет gpu PUBLIC-связью, и второе имя дало бы
# `warning: ignoring duplicate libraries` на macOS.
#
# asset_core (и через него platform_core) приходит транзитивно из ach_bundle. Явным его тут не
# держим: CMake дедуплицирует ПРЯМОЙ список, но транзитивное замыкание дописывает ПОСЛЕ него, и
# библиотека, названная обоими способами, попадает на линкер-строку дважды.
function(game_core_target name materials)
  target_sources(${name} PRIVATE ${GAME_CORE_SRC})
  target_include_directories(${name} PRIVATE ${GAME_CORE_DIR} ${CMAKE_SOURCE_DIR}/engine/render)
  target_include_directories(${name} SYSTEM PRIVATE ${stb_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE
    engine_core framework_input framework_graphics flecs_static webgpu ach_plugin Threads::Threads
    asset_gpu ach_bundle ${materials})
  # AUDIO_HAVE_MINIAUDIO — из audio_device (PUBLIC) → audio.cpp компилит реальный путь; без
  # AUDIO_MINIAUDIO — no-op (CI headless).
  if(AUDIO_MINIAUDIO)
    target_link_libraries(${name} PRIVATE audio_core audio_device)
  endif()
endfunction()
