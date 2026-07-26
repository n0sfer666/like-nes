# --- input_core: action-map + coalesce + SPSC (спека #4). Deviceless-гейты — на всех ОС в CI.
# Native-бэкенды (XInput/RawInput · GameController · evdev) + live-демо — под INPUT_NATIVE ниже.
add_library(input_core STATIC action_map.cpp)
target_include_directories(input_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/asset)
target_link_libraries(input_core PUBLIC engine_core Threads::Threads)

add_executable(input_determinism_test input_determinism_test.cpp)
target_link_libraries(input_determinism_test PRIVATE input_core)

add_executable(input_race_test input_race_test.cpp)
target_link_libraries(input_race_test PRIVATE input_core)

add_executable(input_rt_test input_rt_test.cpp)
target_link_libraries(input_rt_test PRIVATE input_core)

add_executable(input_actionmap_test input_actionmap_test.cpp)
target_link_libraries(input_actionmap_test PRIVATE input_core)

# Live-демо: GLFW-пумп (kbd/mouse) + native gamepad (GameController/XInput/evdev) + rumble.
# Реальные устройства → owner-HW (macOS live). OFF в headless CI (нужен дисплей/HID).
if(INPUT_NATIVE)
  if(APPLE)
    set(INPUT_GAMEPAD_SRC source_gamepad_macos.mm)
  elseif(WIN32)
    set(INPUT_GAMEPAD_SRC source_gamepad_win.cpp)
  else()
    set(INPUT_GAMEPAD_SRC source_gamepad_linux.cpp)
  endif()
  add_executable(input_demo input_demo_main.cpp source_glfw.cpp ${INPUT_GAMEPAD_SRC})
  target_link_libraries(input_demo PRIVATE input_core glfw)
  if(APPLE)
    # ARC для .mm: без него createEngine/createPlayer (autoreleased) повисают → UAF + утечки.
    set_source_files_properties(source_gamepad_macos.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(input_demo PRIVATE "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
  elseif(WIN32)
    target_link_libraries(input_demo PRIVATE Xinput9_1_0)
  endif()
endif()
