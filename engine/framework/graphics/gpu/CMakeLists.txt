# Квады бэкенда из отсортированного списка спрайтов слоёв `LNVL`.
add_library(framework_graphics_gpu STATIC layer_quads.cpp)
target_include_directories(framework_graphics_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_graphics_gpu PUBLIC framework_graphics_tiles render_core)

# Пиксельный голден слоёв на живом устройстве, headless, на трёх ОС; эталон — CPU.
add_executable(framework_layer_golden
  layer_golden_main.cpp layer_golden_scene.cpp layer_golden_ref.cpp)
target_link_libraries(framework_layer_golden PRIVATE framework_graphics_gpu webgpu)
target_include_directories(framework_layer_golden PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
target_copy_webgpu_binaries(framework_layer_golden)
reproducible_rpath(framework_layer_golden)
