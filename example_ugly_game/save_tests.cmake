# Persistence integration: actual game path policy, no user data or GPU required.
add_executable(game_save_paths_test save_paths_test.cpp save_paths.cpp
  ${CMAKE_SOURCE_DIR}/engine/framework/input/profile_file.cpp)
target_include_directories(game_save_paths_test PRIVATE ${CMAKE_SOURCE_DIR}/engine/core
  ${CMAKE_SOURCE_DIR}/engine/input ${CMAKE_SOURCE_DIR}/engine/platform
  ${CMAKE_SOURCE_DIR}/engine/framework/input ${CMAKE_SOURCE_DIR}/engine/framework/save)

add_library(ach_recovery_atomic OBJECT ${CMAKE_SOURCE_DIR}/engine/platform/platform_atomic.cpp)
target_compile_definitions(ach_recovery_atomic PRIVATE write_atomic=ach_recovery_write_atomic_real)
target_include_directories(ach_recovery_atomic PRIVATE ${CMAKE_SOURCE_DIR}/engine/platform)
set(LIKE_NES_ACH_RECOVERY_STORE "${CMAKE_CURRENT_SOURCE_DIR}/ach_persistence.cpp" CACHE FILEPATH "Achievement store source for recovery mutation tests")
add_executable(game_ach_recovery_test ach_recovery_test.cpp ach_recovery_io.cpp
  $<TARGET_OBJECTS:ach_recovery_atomic> achievements.cpp ${LIKE_NES_ACH_RECOVERY_STORE}
  save_paths.cpp backend_host.cpp ach_source_bundle.cpp)
target_compile_definitions(game_ach_recovery_test PRIVATE ACH_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/assets/achievements.txt")
target_include_directories(game_ach_recovery_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_ach_recovery_test PRIVATE framework_save framework_input engine_core
  ach_bundle ach_plugin Threads::Threads)
