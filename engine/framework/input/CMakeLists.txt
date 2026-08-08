# --- framework_input: ввод из коробки (спека #14) — форма отклика стика, пресеты, профили.
# Зависимость только вниз: input_core (спека #4) о слое не знает.
add_library(framework_input STATIC
  stick.cpp source_names.cpp preset_parse.cpp preset_pad_parse.cpp preset_bake.cpp
  presets.cpp pad_profile.cpp pad_registry.cpp rebind_store.cpp rebind_session.cpp)
target_include_directories(framework_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_input PUBLIC framework_core input_core PRIVATE platform_core)

add_executable(framework_stick_test framework_stick_test.cpp)
target_link_libraries(framework_stick_test PRIVATE framework_input platform_core)

add_executable(framework_preset_test framework_preset_test.cpp)
target_link_libraries(framework_preset_test PRIVATE framework_input platform_core)

add_executable(framework_pad_test framework_pad_test.cpp)
target_link_libraries(framework_pad_test PRIVATE framework_input platform_core)

add_executable(framework_rebind_test framework_rebind_test.cpp)
target_link_libraries(framework_rebind_test PRIVATE framework_input platform_core)

add_executable(framework_rt_test framework_rt_test.cpp)
target_link_libraries(framework_rt_test PRIVATE framework_input platform_core)

add_executable(framework_golden_test framework_golden_test.cpp)
target_link_libraries(framework_golden_test PRIVATE framework_input platform_core)

# Живой probe гейта 8: реальный пад и реальная клавиатура, поэтому только там, где собран
# нативный ввод (INPUT_NATIVE). В headless CI цели нет — её нечем кормить.
if(INPUT_NATIVE)
  add_executable(framework_input_probe framework_input_probe.cpp probe_report.cpp
                 probe_axis_report.cpp)
  target_link_libraries(framework_input_probe PRIVATE framework_input input_native platform_core)
endif()
