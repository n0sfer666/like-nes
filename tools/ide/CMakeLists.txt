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

# Гейт 5 (парсер диагностик), гейт 7 (перф 10k) и data-путь гейта 6 — чистая логика без вызовов
# ОС. В POSIX-блок ниже они попали только вложенностью; спека #12 (гейт 9) это снимает: гоняются
# на 3 ОС. Формата теперь два (clang/gcc и MSVC, решение 3 спеки #13) — по единице трансляции на
# каждый, диспетчер строк в diagnostics.cpp.
add_library(diag_core STATIC
  compile/diagnostics.cpp compile/diagnostics_gnu.cpp compile/diagnostics_msvc.cpp)
target_include_directories(diag_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(diagnostics_test compile/diagnostics_test.cpp)
target_link_libraries(diagnostics_test PRIVATE diag_core)

# Гейт 2 спеки #18 обещает диагностику, СОВМЕСТИМУЮ с этой панелью. Обещание проверяется здесь:
# строку печатает валидатор материалов, разбирает её парсер выше, и обе стороны линкуются в одну
# цель. Ни webgpu, ни устройства — формат строки от них не зависит.
add_executable(material_diag_compat_test material_diag_compat_test.cpp)
target_link_libraries(material_diag_compat_test PRIVATE diag_core material_diag)

add_executable(perf_test perf_test.cpp)
target_link_libraries(perf_test PRIVATE scene_core platform_core)   # шов: PLATFORM_NOINLINE

add_library(editor_core STATIC editor/property_grid.cpp)
target_include_directories(editor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(editor_core PUBLIC scene_core)
add_executable(editor_selftest editor/editor_selftest.cpp)
target_link_libraries(editor_selftest PRIVATE editor_core)

# Заголовки зеркала (mirror/seqlock) — чистые структуры и атомики, без вызовов ОС; разделяемую
# память под ними даёт шов platform_shmem (спека #13). Отдельной ipc-библиотеки больше нет:
# после переезда шва в engine/platform в ней остались бы одни заголовки.
add_library(ipc_headers INTERFACE)
target_include_directories(ipc_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ide_ipc_selftest ipc_selftest.cpp)
target_link_libraries(ide_ipc_selftest PRIVATE ipc_headers Threads::Threads)

add_executable(game_child game_child.cpp)
target_link_libraries(game_child PRIVATE ipc_headers platform_core Threads::Threads)

# Гейты 3/4 + layout-drift: parent запускает game_child (путь → argv[1]). После переезда на
# platform::Child и platform_shmem POSIX-вызовов здесь не осталось — цель собирается на трёх ОС.
add_executable(play_spawn_test play_spawn_test.cpp)
target_link_libraries(play_spawn_test PRIVATE ipc_headers platform_core Threads::Threads)

# Оркестратор сборки после переезда на platform::run_capture своих fork/pipe не имеет — цель
# уехала из POSIX-ветки, чтобы MSVC проверял её на всех трёх ОС. Внутри остался только сам
# гейт 5: hot-reload там всё ещё dlopen напрямую.
add_library(build_core STATIC compile/build_orchestrator.cpp)
target_include_directories(build_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(build_core PUBLIC diag_core platform_core)

# Гейт 5: build-loop — watch .cpp → build (реальный компилятор) → hot-reload модуля + панель
# ошибок. Наблюдение через platform_watch, загрузка через platform_module — POSIX-вызовов в самом
# тесте не осталось, и цель собирается на трёх ОС.
#
# Различие компиляторов вынесено СЮДА строкой, а не в код условной компиляцией: тест подставляет
# {src}/{obj}/{out} в готовый шаблон и не знает, на какой он ОС. Разделитель — `|`,
# а не `;`: точка с запятой в CMake это разделитель СПИСКА, и строка разъехалась бы на
# семь отдельных -D ещё до компилятора. MSVC требует ещё и явного
# __declspec(dllexport) — без него в DLL нет ни одного символа, и hot-reload читался бы как
# «сборка прошла, а step пропал».
if(MSVC)
  set(IDE_BL_ARGV "${CMAKE_CXX_COMPILER}|/nologo|/EHsc|/LD|{src}|/Fo:{obj}|/Fe:{out}")
  set(IDE_BL_EXPORT "__declspec(dllexport) ")
else()
  set(IDE_BL_ARGV "${CMAKE_CXX_COMPILER}|-shared|-fPIC|-O2|{src}|-o|{out}")
  set(IDE_BL_EXPORT "")
endif()

add_executable(build_loop_test build_loop_test.cpp)
target_link_libraries(build_loop_test PRIVATE build_core platform_core Threads::Threads)
target_compile_definitions(build_loop_test PRIVATE
  IDE_BUILD_ARGV="${IDE_BL_ARGV}"
  IDE_EXPORT="${IDE_BL_EXPORT}"
  IDE_MODULE_SUFFIX="${CMAKE_SHARED_LIBRARY_SUFFIX}")

# Live editor-shell (гейт 6): вьюпорт+гизмо+инспектор+иерархия в ImGui docking (#6). Owner-HW.
# Собирается когда доступен imgui-таргет (PLUGIN_UI on); в headless CI PLUGIN_UI=OFF → пропуск.
if(TARGET imgui)
  # WebGPU-бэкенд + оконная обвязка — в imgui-либе (engine/render/wgpu_imgui). GpuContext и
  # capture (снимок кадра для --gate6) — из render_core; glfw3webgpu — surface из GLFW-окна.
  # platform_core нужен режиму гейта: паспорт сессии читает окружение через platform::env_var.
  add_executable(editor_shell
    editor/editor_shell_main.cpp editor/editor_gate6.cpp editor/editor_gate6_session.cpp)
  target_link_libraries(editor_shell PRIVATE editor_core imgui render_core platform_core glfw3webgpu webgpu)
  target_copy_webgpu_binaries(editor_shell)
  reproducible_rpath(editor_shell)
endif()

# Единственная оставшаяся POSIX-цель дерева, и это НЕ временное состояние: вторая половина
# замера — unix-socket (sys/socket.h), а сам замер свою работу уже сделал — транспорт выбран в
# ADR 0007 (shmem ~17×). Порт на Winsock повторял бы измерение, вывод которого принят, ради
# буквы инварианта; сам транспорт (shmem) закрыт швом и гоняется на трёх ОС. Граница названа
# явно в ADR 0013, а не оставлена дырой.
if(NOT WIN32)
  add_executable(ide_ipc_bench ipc_bench.cpp)
  target_link_libraries(ide_ipc_bench PRIVATE ipc_headers Threads::Threads)
endif()
