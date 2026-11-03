# Сквозной RT/allocator probe использует тестовый helper слоя, а audio_core от слоя не зависит.
add_executable(audio_stereo_limits_test audio_stereo_limits_test.cpp)
target_link_libraries(audio_stereo_limits_test PRIVATE audio_core framework_alloc_probe_control)
target_include_directories(audio_stereo_limits_test PRIVATE
  $<TARGET_PROPERTY:framework_core,INTERFACE_INCLUDE_DIRECTORIES>
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
