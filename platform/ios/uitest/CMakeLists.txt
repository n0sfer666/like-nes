# UI-тест гейта симулятора (`scripts/ios_sim_gate.sh`). Бандл `.xctest` без TEST_HOST: UI-тест идёт
# ОТДЕЛЬНЫМ процессом-раннером и трогает приложение снаружи, через UIKit, как палец.
add_library(like_nes_ios_uitests MODULE ${CMAKE_CURRENT_SOURCE_DIR}/touch_gate.m)
set_target_properties(like_nes_ios_uitests PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION xctest
  XCTEST TRUE
  XCTEST_TESTEE like_nes_ios
  XCODE_PRODUCT_TYPE com.apple.product-type.bundle.ui-testing
  XCODE_ATTRIBUTE_TEST_TARGET_NAME like_nes_ios
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.likenes.sidescroller.uitests
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
# Цель вне game_core_target, и флаги дерева на неё не ложатся: ноль предупреждений держим тут.
target_compile_options(like_nes_ios_uitests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(like_nes_ios_uitests PRIVATE "-framework XCTest" "-framework UIKit")
add_dependencies(like_nes_ios_uitests like_nes_ios)
# Схему с test action CMake пишет ТЕСТИРУЕМОЙ цели (по XCTEST_TESTEE): гейт зовёт `-scheme like_nes_ios`.
set_property(TARGET like_nes_ios PROPERTY XCODE_GENERATE_SCHEME ON)
