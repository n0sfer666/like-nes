# --- IDE-вертикаль (спека #7) — фундамент: flecs scene-doc + детерм. round-trip + undo/redo ---
# Гейт 1 (scene round-trip): текст-source(JSON)+стабильные GUID → save->reload->save байт-идентичны +
# детерм. golden bake-hash (fix32 целочисл. → cross-arch). Гейт 2 (undo/redo): command-bus. На 3 OS.
if(NOT IDE_POC)
  return()
endif()

add_library(scene_core STATIC scene.cpp serialize.cpp)
target_include_directories(scene_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scene_core PUBLIC flecs_static engine_core)

add_executable(scene_roundtrip_test scene_roundtrip_test.cpp)
target_link_libraries(scene_roundtrip_test PRIVATE scene_core)

add_executable(command_test command_test.cpp)
target_link_libraries(command_test PRIVATE scene_core)

# IPC-вертикаль (гейты 3/4/8): Play-процесс + read-only shmem-зеркало + крэш-изоляция + bench.
# POSIX (shm_open/mmap/fork). Windows — follow-up (код+CI-build, как #6 SEH).
if(NOT WIN32)
  add_library(ipc_core STATIC ipc/shmem.cpp)
  target_include_directories(ipc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  if(NOT APPLE)
    target_link_libraries(ipc_core PUBLIC rt)   # shm_open на Linux — в librt
  endif()

  add_executable(ide_ipc_selftest ipc_selftest.cpp)
  target_link_libraries(ide_ipc_selftest PRIVATE ipc_core Threads::Threads)

  add_executable(game_child game_child.cpp)
  target_link_libraries(game_child PRIVATE ipc_core Threads::Threads)

  # Гейты 3/4 + layout-drift: parent spawn'ит game_child (путь → argv[1]).
  add_executable(play_spawn_test play_spawn_test.cpp)
  target_link_libraries(play_spawn_test PRIVATE ipc_core Threads::Threads)

  # Гейт 8: замер shmem vs socket @10k → выбор транспорта в ADR 0007.
  add_executable(ide_ipc_bench ipc_bench.cpp)
  target_link_libraries(ide_ipc_bench PRIVATE ipc_core Threads::Threads)

  # Гейт 5: build-loop — watch .cpp → build (реальный компилятор) → hot-reload .so + панель ошибок.
  # Переиспользует dlopen (hot-reload #1). Компилятор → argv[1] (CI: ${CMAKE_CXX_COMPILER}).
  add_library(build_core STATIC compile/diagnostics.cpp compile/build_orchestrator.cpp)
  target_include_directories(build_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  add_executable(build_loop_test build_loop_test.cpp)
  target_link_libraries(build_loop_test PRIVATE build_core ${CMAKE_DL_LIBS})
  target_compile_definitions(build_loop_test PRIVATE IDE_CXX="${CMAKE_CXX_COMPILER}")

  add_executable(diagnostics_test compile/diagnostics_test.cpp)
  target_link_libraries(diagnostics_test PRIVATE build_core)

  # Гейт 7: перф 10k — выделение/property-grid/undo/виртуализация ≤ бюджет + zero per-frame heap.
  add_executable(perf_test perf_test.cpp)
  target_link_libraries(perf_test PRIVATE scene_core)

  # Гейт 6 data-путь: property-grid из flecs meta + гизмо/камера transform (headless-гейт).
  # Live-окно (вьюпорт+инспектор+гизмо) — editor_shell ниже (owner-HW).
  add_library(editor_core STATIC editor/property_grid.cpp)
  target_include_directories(editor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(editor_core PUBLIC scene_core)
  add_executable(editor_selftest editor/editor_selftest.cpp)
  target_link_libraries(editor_selftest PRIVATE editor_core)

  # Live editor-shell (гейт 6): вьюпорт+гизмо+инспектор+иерархия в ImGui docking (#6). Owner-HW.
  # Собирается когда доступен imgui-таргет (PLUGIN_UI on); в headless CI PLUGIN_UI=OFF → пропуск.
  if(TARGET imgui)
    # WebGPU-бэкенд + оконная обвязка — в imgui-либе (engine/render/wgpu_imgui). GpuContext из
    # engine/render/gpu.cpp; glfw3webgpu — surface из GLFW-окна.
    add_executable(editor_shell editor/editor_shell_main.cpp ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
    target_link_libraries(editor_shell PRIVATE editor_core imgui glfw3webgpu webgpu)
    target_copy_webgpu_binaries(editor_shell)
    reproducible_rpath(editor_shell)
  endif()
endif()
