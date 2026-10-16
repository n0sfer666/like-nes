# Android-оболочка — подкаталог корня, а не свой проект: цели движка, flecs и wgpu-native приходят из
# корня под тулчейном NDK, игра — общим списком example_ugly_game/game_sources.cmake. Своё здесь —
# только native_app_glue-вход и ANativeWindow; APK собирает like_nes_mobile_app
# (cmake/like_nes_mobile_app.cmake), build_apk.sh лишь конфигурирует дерево и зовёт его цель.
include(${CMAKE_SOURCE_DIR}/example_ugly_game/game_sources.cmake)

set(GLUE ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue)
# Чужой C-код NDK: строгие флаги дерева (cmake/warnings.cmake) судят наш код, а не его.
add_library(native_app_glue STATIC ${GLUE}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE})
like_nes_vendored_target(native_app_glue)
like_nes_vendored_headers(native_app_glue)

set(MOBILE ${CMAKE_SOURCE_DIR}/platform/mobile)
set(ANDROID_SHELL native_main.cpp android_host.cpp apk_assets.cpp stdout_logcat.cpp)
list(TRANSFORM ANDROID_SHELL PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

# Оболочка — библиотекой, одной на игру и бенч: вход native_app_glue, ANativeWindow и жизненный
# цикл поверхности не знают, какую mobile::Game им отдаст make_game() цели. gpu.cpp едет здесь, а не
# в игре: контекст поднимает оболочка, до того как игра появится.
add_library(like_nes_mobile STATIC ${ANDROID_SHELL} ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
target_include_directories(like_nes_mobile PUBLIC ${MOBILE} ${CMAKE_SOURCE_DIR}/engine/render)
target_link_libraries(like_nes_mobile PUBLIC webgpu PRIVATE platform_core native_app_glue android log)

include(${MOBILE}/resim_bench.cmake)
if(LIKE_NES_MOBILE_BENCH)
  like_nes_mobile_app(bench SOURCES ${MOBILE}/bench_game.cpp
    BUNDLES ${RESIM_BENCH_AGAINST} NAME "like-nes bench" ID com.likenes.resimbench)
  target_link_libraries(bench PRIVATE resim_bench)
  return()
endif()

# GpuContext — уже в оболочке: вторая копия gpu.cpp в игре дала бы дубль символов на линковке.
list(REMOVE_ITEM GAME_CORE_SRC ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
like_nes_mobile_app(game
  SOURCES ${GAME_CORE_DIR}/shooter_mobile.cpp ${GAME_CORE_DIR}/shooter_touch.cpp
          ${GAME_CORE_DIR}/sim_hash.cpp
  BUNDLES ${GAME_BUNDLES} NAME like-nes ID com.likenes.sidescroller)
game_core_target(game material_gpu)
target_link_libraries(game PRIVATE framework_touch)
