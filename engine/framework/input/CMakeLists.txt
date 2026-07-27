# --- framework_input: ввод из коробки (спека #14) — форма отклика стика, пресеты, профили.
# Зависимость только вниз: input_core (спека #4) о слое не знает.
add_library(framework_input STATIC stick.cpp)
target_include_directories(framework_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_input PUBLIC framework_core input_core)

add_executable(framework_stick_test framework_stick_test.cpp)
target_link_libraries(framework_stick_test PRIVATE framework_input platform_core)
