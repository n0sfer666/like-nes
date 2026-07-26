# --- Render-вертикаль (спека #2): deferred+forward, HDR/bloom, offscreen golden ---
add_library(render_core STATIC
  gpu.cpp arena.cpp scene.cpp sprite.cpp
  gpu_util.cpp shaders_gbuffer.cpp shaders_lighting.cpp
  shaders_post.cpp renderer.cpp renderer_pipelines.cpp
  renderer_forward.cpp renderer_bloom.cpp capture.cpp
  stb_impl.cpp)
target_include_directories(render_core SYSTEM PUBLIC ${stb_SOURCE_DIR})   # stb — сторонний, не варнить
target_link_libraries(render_core PUBLIC webgpu)
# stb_image_write.h использует sprintf (deprecated на macOS) — сторонний код, глушим точечно.
if(NOT MSVC)  # GCC/Clang-only флаг; MSVC → D8021
  set_source_files_properties(stb_impl.cpp PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()

add_executable(render_demo demo_main.cpp)
target_link_libraries(render_demo PRIVATE render_core webgpu glfw3webgpu)   # glfw — транзитивно (без дубля)
target_copy_webgpu_binaries(render_demo)
reproducible_rpath(render_demo)

# Headless T4-гейт: offscreen epsilon/perceptual golden (без окна, для CI).
add_executable(render_golden golden_main.cpp)
target_link_libraries(render_golden PRIVATE render_core webgpu)
target_copy_webgpu_binaries(render_golden)
reproducible_rpath(render_golden)

# Ядро ImGui + glfw + WebGPU-бэкенд + общая оконная обвязка (wgpu_imgui). Оба shell'а
# (plugin + editor) рендерят через WebGPU — без deprecated OpenGL. OFF в headless CI.
if(PLUGIN_UI)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
    wgpu_imgui.cpp)
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(imgui PUBLIC glfw webgpu)
  target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)   # wgpu-native, не Dawn
  # Варнинг memset(non-trivial) в imgui_impl_glfw.cpp — код самого imgui (не наш); глушим точечно.
  target_compile_options(imgui PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-nontrivial-memcall>)
endif()
