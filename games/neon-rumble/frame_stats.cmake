# Shared by the SDK game and the standalone fixture for the private benchmark driver.
function(rumble_frame_stats_sources target)
  target_sources(${target} PRIVATE
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/rumble_frame_stats.cpp"
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/rumble_frame_music.cpp")
  if(TARGET like-nes::audio_device)
    target_link_libraries(${target} PRIVATE like-nes::audio_device)
  endif()
endfunction()
