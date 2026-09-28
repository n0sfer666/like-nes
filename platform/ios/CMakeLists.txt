# iOS-оболочка — подкаталог корня, а не свой проект: цели движка, flecs и wgpu-native приходят из
# корня под тем же тулчейном, игра — общим списком example_ugly_game/game_sources.cmake. Своё здесь —
# только UIKit-вход, слой Metal и упаковка `.app`.
include(${CMAKE_SOURCE_DIR}/cmake/licenses.cmake)
include(${CMAKE_SOURCE_DIR}/example_ugly_game/game_sources.cmake)

set(MOBILE ${CMAKE_SOURCE_DIR}/platform/mobile)
# Бандлы игры кладутся в `assets/` внутри `.app`: `resolve_asset` ищет `<exe_dir>/assets/<имя>`, а
# у iOS-приложения exe_dir и есть корень бандла — оболочке не нужно подсовывать путь окружением.
set(GAME_BUNDLES game.bundle library.bundle audio.bundle)
list(TRANSFORM GAME_BUNDLES PREPEND ${CMAKE_SOURCE_DIR}/example_ugly_game/assets/)

add_executable(like_nes_ios MACOSX_BUNDLE
  ${LICENSE_FILES} ${GAME_BUNDLES}
  ${CMAKE_CURRENT_SOURCE_DIR}/app.mm ${CMAKE_CURRENT_SOURCE_DIR}/view.mm ${MOBILE}/mobile_game.cpp)
game_core_target(like_nes_ios material_gpu)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/app.mm ${CMAKE_CURRENT_SOURCE_DIR}/view.mm
  PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_include_directories(like_nes_ios PRIVATE ${MOBILE})
target_link_libraries(like_nes_ios PRIVATE
  "-framework UIKit" "-framework Metal" "-framework QuartzCore"
  "-framework Foundation" "-framework CoreGraphics")

set_source_files_properties(${LICENSE_FILES} PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources/licenses HEADER_FILE_ONLY ON)
set_source_files_properties(${GAME_BUNDLES} PROPERTIES
  MACOSX_PACKAGE_LOCATION assets HEADER_FILE_ONLY ON)

set_target_properties(like_nes_ios PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
  MACOSX_BUNDLE_EXECUTABLE_NAME like_nes_ios
  MACOSX_BUNDLE_GUI_IDENTIFIER com.likenes.sidescroller
  MACOSX_BUNDLE_BUNDLE_NAME like-nes)
