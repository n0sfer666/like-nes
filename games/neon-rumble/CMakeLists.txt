cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)
add_subdirectory(sim)

set(RUMBLE_SOURCES src/rumble.cpp src/rumble_level.cpp src/rumble_layers.cpp
  src/rumble_fighter.cpp src/rumble_fighter_quads.cpp src/rumble_credits.cpp src/rumble_brawl.cpp
  src/rumble_roster_quads.cpp src/rumble_report.cpp src/rumble_brawl_report.cpp src/rumble_kinds.cpp
  src/rumble_depth_overlay.cpp src/rumble_controls.cpp src/rumble_hotseat.cpp src/rumble_banner.cpp
  src/rumble_gpu_frame.cpp src/rumble_wave.cpp src/rumble_wave_read.cpp src/rumble_facing.cpp
  src/rumble_brain.cpp src/rumble_wave_script.cpp src/rumble_steer.cpp
  src/rumble_menu.cpp src/rumble_menu_quads.cpp src/rumble_play.cpp src/rumble_flow.cpp
  src/rumble_screen.cpp src/rumble_clipless.cpp src/rumble_crates.cpp src/rumble_knives.cpp
  src/rumble_platform.cpp src/rumble_stage_quads.cpp src/rumble_label.cpp src/rumble_coop_script.cpp
  src/rumble_join_report.cpp src/rumble_demo.cpp)
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  like_nes_mobile_app(neon_rumble SOURCES src/rumble_mobile.cpp ${RUMBLE_SOURCES}
    NAME "Neon Rumble" ID com.likenes.neonrumble)
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine)
else()
  add_executable(neon_rumble src/main.cpp src/rumble_window.cpp src/rumble_keys.cpp src/rumble_pad_report.cpp
    src/rumble_window_script.cpp src/rumble_shot.cpp
    ${RUMBLE_SOURCES})
  target_link_libraries(neon_rumble PRIVATE rumble_sim like-nes::engine like-nes::window)
  like_nes_add_game(neon_rumble)
endif()
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
