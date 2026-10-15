# iOS-оболочка — подкаталог корня, а не свой проект: цели движка, flecs и wgpu-native приходят из
# корня под тем же тулчейном, игра — общим списком example_ugly_game/game_sources.cmake. Своё здесь —
# только UIKit-вход, слой Metal и упаковка `.app`.
include(${CMAKE_SOURCE_DIR}/cmake/licenses.cmake)
include(${CMAKE_SOURCE_DIR}/example_ugly_game/game_sources.cmake)

# Пустая цель дала бы plist с пустым MinimumOSVersion и cargo без цели — провал поздний и немой.
# Так бывает, если iOS задан toolchain-файлом: platform/mobile/ios_min_os.cmake видит его до project().
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  message(FATAL_ERROR "iOS shell: CMAKE_OSX_DEPLOYMENT_TARGET is empty; pass -DCMAKE_SYSTEM_NAME=iOS or set it explicitly")
endif()

set(MOBILE ${CMAKE_SOURCE_DIR}/platform/mobile)
set_source_files_properties(${LICENSE_FILES} PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources/licenses HEADER_FILE_ONLY ON)

function(ios_bundle target id name)
  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    MACOSX_BUNDLE_EXECUTABLE_NAME ${target}
    MACOSX_BUNDLE_GUI_IDENTIFIER ${id}
    MACOSX_BUNDLE_BUNDLE_NAME ${name}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${id})
endfunction()

# Подменяется только живой самопроверкой гейта симулятора: мутант view.mm обязан уронить гейт.
set(LIKE_NES_IOS_VIEW ${CMAKE_CURRENT_SOURCE_DIR}/view.mm CACHE FILEPATH "UIKit view source of the iOS shell")

# Оболочка — библиотекой, одной на игру и бенч: вход UIKit, слой Metal и жизненный цикл поверхности
# не знают, какую mobile::Game им отдаст make_game() цели. gpu.cpp едет здесь, а не в игре: контекст
# поднимает оболочка, до того как игра появится.
set(SHELL_SRC app.mm host.mm metal_view.mm weak_proxy.mm)
list(TRANSFORM SHELL_SRC PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
list(APPEND SHELL_SRC ${LIKE_NES_IOS_VIEW})
add_library(like_nes_mobile STATIC ${SHELL_SRC} ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
set_source_files_properties(${SHELL_SRC} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_include_directories(like_nes_mobile PUBLIC ${MOBILE} ${CMAKE_SOURCE_DIR}/engine/render)
# Свой каталог — явно: мутант view.mm живой самопроверки лежит вне него, а заголовки ищет здесь.
target_include_directories(like_nes_mobile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(like_nes_mobile PUBLIC webgpu
  "-framework UIKit" "-framework Metal" "-framework QuartzCore"
  "-framework Foundation" "-framework CoreGraphics")

include(${MOBILE}/resim_bench.cmake)
if(LIKE_NES_MOBILE_BENCH)
  add_executable(like_nes_resim_bench MACOSX_BUNDLE
    ${LICENSE_FILES} ${RESIM_BENCH_AGAINST} ${MOBILE}/bench_game.cpp)
  set_source_files_properties(${RESIM_BENCH_AGAINST} PROPERTIES MACOSX_PACKAGE_LOCATION assets HEADER_FILE_ONLY ON)
  target_link_libraries(like_nes_resim_bench PRIVATE like_nes_mobile resim_bench)
  ios_bundle(like_nes_resim_bench com.likenes.resimbench like-nes-bench)
  set_target_properties(like_nes_resim_bench PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic)
  return()
endif()

# Бандлы игры кладутся в `assets/` внутри `.app`: `resolve_asset` ищет `<exe_dir>/assets/<имя>`, а
# у iOS-приложения exe_dir и есть корень бандла — оболочке не нужно подсовывать путь окружением.
set(GAME_BUNDLES game.bundle library.bundle audio.bundle)
list(TRANSFORM GAME_BUNDLES PREPEND ${CMAKE_SOURCE_DIR}/example_ugly_game/assets/)

# GpuContext — уже в оболочке: вторая копия gpu.cpp в игре дала бы дубль символов на линковке.
list(REMOVE_ITEM GAME_CORE_SRC ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
add_executable(like_nes_ios MACOSX_BUNDLE
  ${LICENSE_FILES} ${GAME_BUNDLES}
  ${GAME_CORE_DIR}/shooter_mobile.cpp ${GAME_CORE_DIR}/shooter_touch.cpp
  ${GAME_CORE_DIR}/sim_hash.cpp)
game_core_target(like_nes_ios material_gpu)
target_link_libraries(like_nes_ios PRIVATE like_nes_mobile)

set_source_files_properties(${GAME_BUNDLES} PROPERTIES
  MACOSX_PACKAGE_LOCATION assets HEADER_FILE_ONLY ON)

ios_bundle(like_nes_ios com.likenes.sidescroller like-nes)

# UI-тест живёт только в проекте Xcode: раннер XCUITest собирает и ставит xcodebuild, у Ninja его нет.
if(CMAKE_GENERATOR STREQUAL "Xcode")
  add_subdirectory(uitest)
endif()
