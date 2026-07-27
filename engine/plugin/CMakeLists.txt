# --- plugin_core: ExtensionPointRegistry + PluginHost (C-ABI native + WASM-точка) + встроенные
# sim-системы (спека #6). Загрузка модуля и крэш-изоляция — за швами platform_core (#12):
# dlopen/siglongjmp на POSIX, LoadLibraryExW+копия/SEH на Windows. Гейты determinism/seam/
# isolation — на всех 3 OS. platform_core PUBLIC: Module — поле PluginHost.
add_library(plugin_core STATIC registry.cpp host.cpp builtin.cpp manifest.cpp)
target_include_directories(plugin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plugin_core PUBLIC engine_core platform_core)

add_executable(plugin_determinism_test plugin_determinism_test.cpp)
target_link_libraries(plugin_determinism_test PRIVATE plugin_core)

add_executable(plugin_seam_test plugin_seam_test.cpp)
target_link_libraries(plugin_seam_test PRIVATE plugin_core)

add_executable(plugin_isolation_test plugin_isolation_test.cpp)
target_link_libraries(plugin_isolation_test PRIVATE plugin_core)

# Headless-гейт конфигурируемого интерфейса: манифест (данные) → панели/докинг. Без окна, в CI.
add_executable(plugin_manifest_test plugin_manifest_test.cpp)
target_link_libraries(plugin_manifest_test PRIVATE plugin_core)

# Native C-ABI плагины как MODULE (.so/.dll без lib-префикса). crash на -O0, чтобы null-deref
# не соптимизировался (как game_crash в hot-reload). ach_fake/steam/ach_v1 — бэкенды достижений
# на точке расширения #6 (спека #10); ach_v1 намеренно с заниженной ABI-версией.
add_library(plugin_gravity MODULE plugins/gravity.cpp)
add_library(plugin_wind MODULE plugins/wind.cpp)
add_library(plugin_rle_codec MODULE plugins/rle_codec.cpp)
add_library(plugin_multi MODULE plugins/multi.cpp)
add_library(plugin_crash MODULE plugins/crash.cpp)
add_library(plugin_ach_fake MODULE plugins/ach_fake.cpp)
add_library(plugin_steam MODULE plugins/steam_backend.cpp)
add_library(plugin_ach_v1 MODULE plugins/ach_v1.cpp)
# plugin_crash оптимизируется как все: намеренный сбой записан через volatile (запись —
# наблюдаемый эффект, компилятор обязан её выпустить). Прежнее /Od поверх релизного /O2 давало
# на MSVC предупреждение драйвера D9025, а сборочный гейт предупреждений не терпит. Что фикстура
# действительно падает, проверяет гейт 4 на трёх ОС, а не наша вера в флаг.
foreach(P plugin_gravity plugin_wind plugin_rle_codec plugin_multi plugin_crash
          plugin_ach_fake plugin_steam plugin_ach_v1)
  # platform_core, а не только его заголовки: фикстуры читают свои ручки через platform::env_var
  # (getenv отдаёт на Windows ANSI-строку и помечен небезопасным). Статическая библиотека —
  # неиспользованные объекты в модуль не попадают, поэтому плагинам без окружения это ничего не стоит.
  target_include_directories(${P} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/platform)
  target_link_libraries(${P} PRIVATE engine_core platform_core)
  set_target_properties(${P} PROPERTIES PREFIX "")
endforeach()

# Реальный Steamworks не вендорится (спека #9): SDK подключается только явным путём
# у владельца билда, CI гоняет тот же адаптер против контрактного стаба.
set(STEAMWORKS_SDK_DIR "" CACHE PATH "Path to a local Steamworks SDK (empty = contract stub)")
if(STEAMWORKS_SDK_DIR)
  target_compile_definitions(plugin_steam PRIVATE LIKE_NES_STEAM_SDK)
  target_include_directories(plugin_steam PRIVATE ${STEAMWORKS_SDK_DIR}/public)
  if(APPLE)
    target_link_libraries(plugin_steam PRIVATE ${STEAMWORKS_SDK_DIR}/redistributable_bin/osx/libsteam_api.dylib)
  elseif(WIN32)
    target_link_libraries(plugin_steam PRIVATE ${STEAMWORKS_SDK_DIR}/redistributable_bin/win64/steam_api64.lib)
  else()
    target_link_libraries(plugin_steam PRIVATE ${STEAMWORKS_SDK_DIR}/redistributable_bin/linux64/libsteam_api.so)
  endif()
endif()

# Live UI-shell: Dear ImGui (docking) + GLFW + WebGPU (wgpu-native, как render #2 — OpenGL deprecated
# на macOS). Докируемые панели из манифестов. Owner-HW (macOS live). OFF в headless CI (нужен дисплей).
if(PLUGIN_UI)
  add_executable(plugin_ui_shell ui_shell_main.cpp ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
  target_link_libraries(plugin_ui_shell PRIVATE plugin_core imgui glfw3webgpu webgpu)
  target_copy_webgpu_binaries(plugin_ui_shell)
  reproducible_rpath(plugin_ui_shell)
endif()

# WASM-sandbox гейт (спека #6, gate #5): untrusted-плагин в wasmtime-песочнице. Guest — WAT
# (i32 fix32, детерм.), грузится через wasmtime_wat2wasm (без wasm-ld). Local pinned-T4 (C-API
# prebuilt в deps/, не в git; CI — follow-up). Гейты: native≡WASM sim-hash + OOB/cap escape→trap.
if(PLUGIN_WASM)
  file(GLOB WASMTIME_DIR ${CMAKE_SOURCE_DIR}/deps/wasmtime-*-c-api)
  if(WASMTIME_DIR AND EXISTS ${WASMTIME_DIR}/include/wasmtime.h)
    add_executable(plugin_wasm_test plugin_wasm_test.cpp wasm_host.cpp)
    target_include_directories(plugin_wasm_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR} ${WASMTIME_DIR}/include)
    target_link_libraries(plugin_wasm_test PRIVATE plugin_core engine_core ${WASMTIME_DIR}/lib/libwasmtime.a)
    if(APPLE)
      target_link_libraries(plugin_wasm_test PRIVATE
        "-framework CoreFoundation" "-framework Security" "-framework SystemConfiguration")
    elseif(UNIX)
      target_link_libraries(plugin_wasm_test PRIVATE pthread dl m)
    endif()
  else()
    message(STATUS "PLUGIN_WASM: wasmtime C-API не найден в deps/ — gate #5 пропущен")
  endif()
endif()
