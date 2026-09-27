# Android-оболочка — подкаталог корня, а не свой проект: цели движка, flecs и wgpu-native приходят из
# корня под тулчейном NDK, игра — общим списком example_ugly_game/game_sources.cmake. Своё здесь —
# только native_app_glue-вход и ANativeWindow; упаковку APK (бандлы в assets/) делает build_apk.sh.
include(${CMAKE_SOURCE_DIR}/example_ugly_game/game_sources.cmake)

set(GLUE ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue)
# Чужой C-код NDK: строгие флаги дерева (cmake/warnings.cmake) судят наш код, а не его.
add_library(native_app_glue STATIC ${GLUE}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE})
like_nes_vendored_target(native_app_glue)
like_nes_vendored_headers(native_app_glue)

add_library(game SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/native_main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/apk_assets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/stdout_logcat.cpp
  ${CMAKE_SOURCE_DIR}/platform/mobile/mobile_game.cpp)
game_core_target(game material_gpu)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR}/platform/mobile)
target_link_libraries(game PRIVATE native_app_glue android log)
target_link_options(game PRIVATE -u ANativeActivity_onCreate)
