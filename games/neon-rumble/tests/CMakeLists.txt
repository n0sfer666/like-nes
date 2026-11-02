foreach(part level level_refusal)
  add_executable(framework_scene_${part}_test framework_scene_${part}_test.cpp)
  target_link_libraries(framework_scene_${part}_test PRIVATE rumble_sim asset_core)
endforeach()
