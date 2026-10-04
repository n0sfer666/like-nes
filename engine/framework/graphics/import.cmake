add_library(framework_graphics_import STATIC clip_parts.cpp aseprite_parse.cpp aseprite_frames.cpp
  aseprite_tags.cpp aseprite_slices.cpp aseprite_clip.cpp sheet_parse.cpp layer_cover.cpp)
target_include_directories(framework_graphics_import PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_graphics_import PUBLIC framework_graphics framework_tiled)

foreach(t framework_graphics_clip_bake_test framework_graphics_clip_import_test
    framework_graphics_clip_import_refusal_test framework_graphics_clip_read_test
    framework_graphics_layer_cover_test)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE framework_graphics_import)
  target_include_directories(${t} PRIVATE
    $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
endforeach()
