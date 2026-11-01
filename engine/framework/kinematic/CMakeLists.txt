add_library(framework_kinematic STATIC path_follow.cpp movers.cpp movers_snapshot.cpp movers_hash.cpp)
target_include_directories(framework_kinematic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_kinematic PUBLIC framework_physics framework_scene)

foreach(part path movers)
  add_executable(framework_kinematic_${part}_test framework_kinematic_${part}_test.cpp)
  target_link_libraries(framework_kinematic_${part}_test PRIVATE framework_kinematic)
endforeach()
