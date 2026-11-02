add_library(framework_haptic STATIC haptic.cpp)
target_include_directories(framework_haptic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_haptic PRIVATE input_gamepad platform_haptic)

add_executable(haptic_test haptic_test.cpp haptic.cpp)
target_include_directories(haptic_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/platform)
target_link_libraries(haptic_test PRIVATE input_core platform_core)
