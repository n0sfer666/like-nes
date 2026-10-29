# --- framework_depth: тело belt на плоскости пола с глубиной (спека #29, В2б) — пол со стенами, ход
# по осям, тяжесть по `y` и лента. Жанра здесь нет: «когда прыжок» и разворот лица решает игра.
add_library(framework_depth STATIC depth_floor.cpp depth_move.cpp belt_ride.cpp)
target_include_directories(framework_depth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_depth PUBLIC framework_core)

add_executable(framework_depth_floor_test framework_depth_floor_test.cpp)
target_link_libraries(framework_depth_floor_test PRIVATE framework_depth platform_core)

add_executable(framework_depth_move_test framework_depth_move_test.cpp)
target_link_libraries(framework_depth_move_test PRIVATE framework_depth platform_core)
