# --- framework_touch: экранный тач как устройство ввода (спека #26 В2б) — раскладка LNTL и источник
# событий. Зависимость только вниз: ядро ввода о раскладке на экране не знает.
add_library(framework_touch STATIC touch_layout.cpp touch_section.cpp touch_source.cpp)
target_include_directories(framework_touch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_touch PUBLIC framework_core input_core)

add_executable(framework_touch_layout_test framework_touch_layout_test.cpp)
target_link_libraries(framework_touch_layout_test PRIVATE framework_touch platform_core)

add_executable(framework_touch_source_test framework_touch_source_test.cpp)
target_link_libraries(framework_touch_source_test PRIVATE framework_touch platform_core)
