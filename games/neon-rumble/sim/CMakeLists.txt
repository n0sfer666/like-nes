include(${CMAKE_CURRENT_LIST_DIR}/fighter.cmake)
add_library(rumble_sim STATIC rumble_floor.cpp rumble_stage_load.cpp rumble_stage_fighters.cpp rumble_stage_error.cpp
  level_objects_read.cpp scene_links.cpp body_pool.cpp body_hash.cpp depth_step.cpp
  archetype.cpp archetype_chain.cpp body_clip.cpp body_react.cpp body_chain.cpp body_run.cpp body_guard.cpp hit_collect.cpp hit_grab.cpp hit_apply.cpp brawl_step.cpp
  seat_step.cpp stage_step.cpp stage_camera_hash.cpp wave_step.cpp next_arena.cpp stage_waves_hash.cpp
  stage_sections.cpp stage_sections_hash.cpp platform_body.cpp section_mode.cpp section_exit.cpp section_step.cpp
  autoscroll_step.cpp stage_leash.cpp level_clamp.cpp checkpoint_step.cpp progress_hash.cpp death_step.cpp
  stage_movers.cpp conveyor_step.cpp
  rope_ride.cpp stage_ropes.cpp stage_ropes_hash.cpp body_kind.cpp clipless_kind.cpp projectile.cpp
  attack_tokens.cpp ai_brain.cpp ai_state.cpp ai_step.cpp ai_target.cpp)
target_include_directories(rumble_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rumble_sim PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(TARGET like-nes::engine)
  target_link_libraries(rumble_sim PUBLIC rumble_fighter like-nes::engine)
elseif(TARGET framework_depth)
  target_link_libraries(rumble_sim PUBLIC rumble_fighter framework_character framework_physics framework_scene
    framework_kinematic framework_hits framework_input framework_core input_core)
else()
  message(FATAL_ERROR "rumble_sim: neither like-nes::engine nor the engine tree targets; call find_package(like-nes) first")
endif()
