cmake_minimum_required(VERSION 3.21)
project(neon_rumble CXX)

find_package(like-nes 0.1 REQUIRED CONFIG)

add_executable(neon_rumble src/main.cpp src/rumble.cpp src/rumble_level.cpp src/rumble_layers.cpp
  src/rumble_window.cpp src/rumble_fighter.cpp src/rumble_fighter_quads.cpp
  src/rumble_credits.cpp src/rumble_brawl.cpp src/rumble_roster_quads.cpp src/rumble_keys.cpp
  src/rumble_report.cpp src/rumble_brawl_report.cpp src/rumble_kinds.cpp src/rumble_depth_overlay.cpp)
target_link_libraries(neon_rumble PRIVATE like-nes::engine like-nes::window)
like_nes_add_game(neon_rumble)
like_nes_bake(neon_rumble MANIFEST game.manifest OUT game.bundle)
