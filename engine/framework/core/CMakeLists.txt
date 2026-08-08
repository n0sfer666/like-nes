# --- framework_core: стадии тика и расписание систем (спека #14, правила слоя — ../README.md).
# Зависит только от engine_core: стрелка слоя направлена вниз, и проверить это должно быть можно
# по этой строке, не читая код.
add_library(framework_core STATIC schedule.cpp fixmath.cpp)
target_include_directories(framework_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_core PUBLIC engine_core)

add_executable(framework_schedule_test framework_schedule_test.cpp)
target_link_libraries(framework_schedule_test PRIVATE framework_core platform_core)
