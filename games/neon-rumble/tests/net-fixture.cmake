# Standalone fixture uses the same Play and network sources as the installed-SDK game.
add_executable(rumble_network_sim_test EXCLUDE_FROM_ALL tests/rumble_network_sim_test.cpp
  tests/rumble_network_desync_test.cpp
  ${RUMBLE_NETWORK_SOURCES}
  src/rumble.cpp src/rumble_brawl.cpp src/rumble_brain.cpp src/rumble_clipless.cpp src/rumble_crates.cpp
  src/rumble_facing.cpp src/rumble_fighter.cpp src/rumble_kinds.cpp src/rumble_knives.cpp
  src/rumble_level.cpp src/rumble_platform.cpp src/rumble_play.cpp src/rumble_steer.cpp
  src/rumble_wave.cpp src/rumble_wave_read.cpp src/rumble_wave_script.cpp)
target_include_directories(rumble_network_sim_test PRIVATE src)
target_link_libraries(rumble_network_sim_test PRIVATE rumble_sim like-nes::net_core)
set_target_properties(rumble_network_sim_test PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
