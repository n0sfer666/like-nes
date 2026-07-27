# --- Render-вертикаль (спека #2): deferred+forward, HDR/bloom, offscreen golden ---
add_library(render_core STATIC
  gpu.cpp arena.cpp scene.cpp sprite.cpp
  gpu_util.cpp shaders_gbuffer.cpp shaders_lighting.cpp
  shaders_post.cpp renderer.cpp renderer_pipelines.cpp
  renderer_forward.cpp renderer_bloom.cpp capture.cpp)
target_include_directories(render_core SYSTEM PUBLIC ${stb_SOURCE_DIR})   # stb — сторонний, не варнить
target_link_libraries(render_core PUBLIC webgpu)
like_nes_vendored_object(stb_impl_vendored stb_impl.cpp)   # TU целиком — реализация stb, ни строки нашей
target_include_directories(stb_impl_vendored SYSTEM PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(render_core PRIVATE stb_impl_vendored)

add_executable(render_demo demo_main.cpp)
target_link_libraries(render_demo PRIVATE render_core platform_core webgpu glfw3webgpu)   # glfw — транзитивно (без дубля)
target_copy_webgpu_binaries(render_demo)
reproducible_rpath(render_demo)

# Headless T4-гейт: offscreen epsilon/perceptual golden (без окна, для CI).
add_executable(render_golden golden_main.cpp)
target_link_libraries(render_golden PRIVATE render_core platform_core webgpu)
target_copy_webgpu_binaries(render_golden)
reproducible_rpath(render_golden)

# Ядро ImGui + glfw + WebGPU-бэкенд + общая оконная обвязка (wgpu_imgui). Оба shell'а
# (plugin + editor) рендерят через WebGPU — без deprecated OpenGL. OFF в headless CI.
if(PLUGIN_UI)
  # Сам imgui — сторонний; наша в цели только обвязка wgpu_imgui.cpp, она остаётся строгой.
  like_nes_vendored_object(imgui_vendored
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)
  target_include_directories(imgui_vendored SYSTEM PRIVATE
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_compile_definitions(imgui_vendored PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
  # У OBJECT-цели нет шага линковки: отсюда едут заголовки и флаги в компиляцию объектов.
  target_link_libraries(imgui_vendored PRIVATE glfw webgpu)

  add_library(imgui STATIC wgpu_imgui.cpp)
  target_link_libraries(imgui PRIVATE imgui_vendored)
  target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  # SYSTEM: заголовки imgui чужие, а wgpu_imgui.cpp в этой же цели — наш и строгий. Без SYSTEM
  # диагностика из чужого заголовка вменяется нам и валит гейт при любом бампе зависимости.
  target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui PUBLIC glfw webgpu)
  target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)   # wgpu-native, не Dawn
endif()
