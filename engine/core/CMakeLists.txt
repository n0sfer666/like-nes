add_library(engine_core INTERFACE)
target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(core_smoke main.cpp)
target_link_libraries(core_smoke PRIVATE webgpu glfw3webgpu)   # glfw — транзитивно из glfw3webgpu (без дубля)
target_copy_webgpu_binaries(core_smoke)
reproducible_rpath(core_smoke)

# Автономный тест детерминизма (без GPU-зависимостей) — гоняется в CI на всех ОС/компиляторах.
add_executable(determinism_test determinism_test.cpp)
