# --- input_core: action-map + coalesce + SPSC (спека #4). Deviceless-гейты — на всех ОС в CI.
# Native-бэкенды (XInput/RawInput · GameController · evdev) — на любом десктопе, live-демо — под INPUT_NATIVE.
add_library(input_core STATIC action_map.cpp action_shared.cpp)
target_include_directories(input_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(input_core PUBLIC engine_core asset_hash Threads::Threads)

add_executable(input_determinism_test input_determinism_test.cpp)
target_link_libraries(input_determinism_test PRIVATE input_core)

add_executable(input_race_test input_race_test.cpp)
target_link_libraries(input_race_test PRIVATE input_core)

add_executable(input_rt_test input_rt_test.cpp)
target_link_libraries(input_rt_test PRIVATE input_core platform_core)   # шов: PLATFORM_NOINLINE

add_executable(input_actionmap_test input_actionmap_test.cpp)
target_link_libraries(input_actionmap_test PRIVATE input_core)

add_executable(input_players_test input_players_test.cpp)
target_link_libraries(input_players_test PRIVATE input_core)

add_executable(input_layouts_test input_layouts_test.cpp)
target_link_libraries(input_layouts_test PRIVATE input_core)

# Нативный ввод: GLFW-пумп (kbd/mouse) + native gamepad (GameController/XInput/evdev) + rumble.
# Библиотека — на любом десктопе, а не под INPUT_NATIVE: её ставит SDK (like-nes::window), и игра
# против префикса без неё не получила бы ни клавиатуры, ни пада. Под опцией — только live-демо.
if(NOT LIKE_NES_MOBILE)
  if(APPLE)
    set(INPUT_GAMEPAD_SRC source_gamepad_macos.mm)
  elseif(WIN32)
    set(INPUT_GAMEPAD_SRC source_gamepad_win.cpp)
  else()
    set(INPUT_GAMEPAD_SRC source_gamepad_linux.cpp)
  endif()
  # Бэкенды отдельной библиотекой, а не списком исходников у каждого потребителя: probe слоя
  # фреймворка (спека #14) берёт ровно тот же нативный ввод, и второй список платформенных
  # ветвлений разошёлся бы с этим при первой же правке.
  add_library(input_native STATIC source_glfw.cpp ${INPUT_GAMEPAD_SRC})
  target_link_libraries(input_native PUBLIC input_core glfw)
  if(APPLE)
    # ARC для .mm: без него createEngine/createPlayer (autoreleased) повисают → UAF + утечки.
    set_source_files_properties(source_gamepad_macos.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(input_native PUBLIC "-framework GameController" "-framework CoreHaptics" "-framework Foundation")
  elseif(WIN32)
    target_link_libraries(input_native PUBLIC Xinput9_1_0)
  endif()
endif()

# Live-демо: реальные устройства → owner-HW (macOS live). OFF в headless CI (нужен дисплей/HID).
if(INPUT_NATIVE)
  add_executable(input_demo input_demo_main.cpp)
  target_link_libraries(input_demo PRIVATE input_native)
endif()
