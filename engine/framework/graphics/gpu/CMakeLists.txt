# Квады бэкенда из отсортированного списка спрайтов слоёв `LNVL`.
add_library(framework_graphics_gpu STATIC layer_quads.cpp cel_quads.cpp text_quads.cpp)
target_include_directories(framework_graphics_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_graphics_gpu PUBLIC framework_graphics_tiles render_core)

# Пиксельный голден слоёв на живом устройстве, headless, на трёх ОС; эталон — CPU.
add_executable(framework_layer_golden
  layer_golden_main.cpp layer_golden_scene.cpp layer_golden_ref.cpp golden_target.cpp)
target_link_libraries(framework_layer_golden PRIVATE framework_graphics_gpu webgpu)
target_include_directories(framework_layer_golden PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
target_copy_webgpu_binaries(framework_layer_golden)
reproducible_rpath(framework_layer_golden)

# Пиксельный голден кадра клипа и оверлея F3 (В6б): то же устройство, что у голдена слоёв.
add_executable(framework_clip_golden
  clip_golden_main.cpp clip_golden_scene.cpp clip_golden_ref.cpp golden_target.cpp)
target_link_libraries(framework_clip_golden PRIVATE framework_graphics_gpu webgpu)
target_include_directories(framework_clip_golden PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
target_copy_webgpu_binaries(framework_clip_golden)
reproducible_rpath(framework_clip_golden)

add_executable(framework_text_quads_test framework_text_quads_test.cpp)
target_link_libraries(framework_text_quads_test PRIVATE framework_graphics_gpu)
target_copy_webgpu_binaries(framework_text_quads_test)
reproducible_rpath(framework_text_quads_test)
