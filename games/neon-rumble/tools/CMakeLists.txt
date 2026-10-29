# Пекарь Neon Rumble (спека #29, В2а): ядро пекаря + вид записи `fighter`. Три способа сборки:
# целями дерева (корень дерева, рядом с `assetc_fighter_test`), против префикса из сборки игры и
# отдельным проектом хоста — мобильная сборка игры не может запустить то, что собирает под телефон,
# и получает готовый бинарь (`bash scripts/rumble_tool.sh <desktop-prefix> <dir>`).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.21)
  project(rumble_assetc CXX)
  find_package(like-nes 0.1 REQUIRED CONFIG)
endif()

add_executable(rumble_assetc rumble_assetc.cpp fighter_kind.cpp)
set_target_properties(rumble_assetc PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(TARGET assetc_core)
  target_link_libraries(rumble_assetc PRIVATE assetc_core framework_brawl_fighter)
  add_executable(assetc_fighter_test assetc_fighter_test.cpp fighter_kind.cpp)
  target_link_libraries(assetc_fighter_test PRIVATE assetc_core framework_brawl_fighter)
elseif(TARGET like-nes::assetc_core)
  # Каталоги заголовков — у like-nes::engine, но не сама цель: она тянет рендер и рантайм wgpu,
  # которые пекарю не нужны.
  target_link_libraries(rumble_assetc PRIVATE like-nes::assetc_core like-nes::framework_brawl_fighter)
  target_include_directories(rumble_assetc PRIVATE
    $<TARGET_PROPERTY:like-nes::engine,INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_features(rumble_assetc PRIVATE cxx_std_20)
  target_compile_options(rumble_assetc PRIVATE $<TARGET_PROPERTY:like-nes::engine,INTERFACE_COMPILE_OPTIONS>)
  like_nes_require_debug_libs(rumble_assetc)
  # Штамп сборки префикса рядом с бинарём: like_nes_bake мобильной сборки сверяет его со своим SDK.
  file(GENERATE OUTPUT "$<TARGET_FILE:rumble_assetc>.like-nes-build" CONTENT "${_ln_build}\n")
else()
  message(FATAL_ERROR "rumble_assetc: neither the engine tree nor a desktop like-nes prefix with assetc_core; "
                      "build it against a desktop prefix (bash scripts/rumble_tool.sh <desktop-prefix> <dir>)")
endif()
