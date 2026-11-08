cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)
add_subdirectory(sim)

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  like_nes_mobile_app(neon_rumble DISCOVERY SOURCES ${RUMBLE_MOBILE_SOURCES} ${RUMBLE_SOURCES}
    ${RUMBLE_NETWORK_SOURCES} ${RUMBLE_NETWORK_UI_SOURCES}
    NAME "Neon Rumble" ID com.likenes.neonrumble)
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine like-nes::net_core like-nes::platform_discovery)
else()
  add_executable(neon_rumble src/main.cpp src/rumble_window.cpp src/rumble_window_host.cpp
    src/rumble_window_bench.cpp src/rumble_keys.cpp src/rumble_pad_report.cpp
    src/rumble_window_script.cpp src/rumble_shot.cpp
    ${RUMBLE_SOURCES})
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine like-nes::window)
  target_sources(neon_rumble PRIVATE ${RUMBLE_NETWORK_SOURCES} ${RUMBLE_NETWORK_UI_SOURCES}
    src/rumble_net_headless.cpp src/rumble_net_window.cpp)
  target_link_libraries(neon_rumble PRIVATE like-nes::net_core like-nes::platform_discovery)
  like_nes_add_game(neon_rumble)
  include(tests/net-fixture.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/frame_stats.cmake)
rumble_frame_stats_sources(neon_rumble)
# Бойцов печёт инструмент игры (спека #29, В2а). Мобильная сборка его не запускает — бинарь хоста
# приходит готовым путём.
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  if(NOT RUMBLE_ASSETC)
    message(FATAL_ERROR "neon_rumble: a mobile build bakes with the host's rumble_assetc; build it with "
                        "'bash scripts/rumble_tool.sh <desktop-prefix> <dir>' and pass -DRUMBLE_ASSETC=<path it prints>")
  endif()
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL "${RUMBLE_ASSETC}")
else()
  add_subdirectory(tools)
  like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle TOOL rumble_assetc)
endif()
if(TARGET like-nes::audio_device)
  like_nes_bake(neon_rumble MANIFEST bench/frame-stats.manifest OUT frame-stats.bundle)
endif()
