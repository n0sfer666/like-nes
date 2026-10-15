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

set(MOBILE ${CMAKE_SOURCE_DIR}/platform/mobile)
set(ANDROID_SHELL native_main.cpp android_host.cpp apk_assets.cpp stdout_logcat.cpp)
list(TRANSFORM ANDROID_SHELL PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

# Оболочка — библиотекой, одной на игру и бенч: вход native_app_glue, ANativeWindow и жизненный
# цикл поверхности не знают, какую mobile::Game им отдаст make_game() цели. gpu.cpp едет здесь, а не
# в игре: контекст поднимает оболочка, до того как игра появится. Точки входа держатся `-u`:
# статическая библиотека, на которую из .so никто не ссылается, иначе выпала бы из линковки.
add_library(like_nes_mobile STATIC ${ANDROID_SHELL} ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
target_include_directories(like_nes_mobile PUBLIC ${MOBILE} ${CMAKE_SOURCE_DIR}/engine/render)
target_link_libraries(like_nes_mobile PUBLIC webgpu PRIVATE platform_core native_app_glue android log)
target_link_options(like_nes_mobile INTERFACE "SHELL:-u ANativeActivity_onCreate" "SHELL:-u android_main")

include(${MOBILE}/resim_bench.cmake)
if(LIKE_NES_MOBILE_BENCH)
  add_library(bench SHARED ${MOBILE}/bench_game.cpp)
  target_link_libraries(bench PRIVATE like_nes_mobile resim_bench)
  return()
endif()

# GpuContext — уже в оболочке: вторая копия gpu.cpp в игре дала бы дубль символов на линковке.
list(REMOVE_ITEM GAME_CORE_SRC ${CMAKE_SOURCE_DIR}/engine/render/gpu.cpp)
add_library(game SHARED
  ${GAME_CORE_DIR}/shooter_mobile.cpp ${GAME_CORE_DIR}/shooter_touch.cpp
  ${GAME_CORE_DIR}/sim_hash.cpp)
game_core_target(game material_gpu)
target_link_libraries(game PRIVATE like_nes_mobile)
