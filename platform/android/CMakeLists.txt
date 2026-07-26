cmake_minimum_required(VERSION 3.24)
project(like_nes_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(MOBILE ${CMAKE_CURRENT_SOURCE_DIR}/../mobile)

include(${MOBILE}/wgpu_native.cmake)

include(FetchContent)
set(FLECS_SHARED OFF CACHE BOOL "" FORCE)
set(FLECS_STATIC ON CACHE BOOL "" FORCE)
set(FLECS_PIC ON CACHE BOOL "" FORCE)
set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(flecs
  GIT_REPOSITORY https://github.com/SanderMertens/flecs
  GIT_TAG v4.1.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(flecs)

set(GLUE ${CMAKE_ANDROID_NDK}/sources/android/native_app_glue)
add_library(native_app_glue STATIC ${GLUE}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${GLUE})

add_library(game SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/native_main.cpp
  ${MOBILE}/mobile_game.cpp
  ${REPO}/example_ugly_game/art.cpp ${REPO}/example_ugly_game/batch.cpp ${REPO}/example_ugly_game/sim.cpp
  ${REPO}/example_ugly_game/combat.cpp ${REPO}/example_ugly_game/boss.cpp ${REPO}/example_ugly_game/controls.cpp
  ${REPO}/example_ugly_game/draw.cpp ${REPO}/example_ugly_game/fx.cpp ${REPO}/example_ugly_game/bloom.cpp
  ${REPO}/example_ugly_game/audio.cpp
  ${REPO}/engine/input/action_map.cpp ${REPO}/engine/render/gpu.cpp)

target_include_directories(game PRIVATE
  ${REPO}/example_ugly_game ${MOBILE} ${REPO}/engine/core ${REPO}/engine/input ${REPO}/engine/render)
target_link_libraries(game PRIVATE
  native_app_glue wgpu_native flecs_static android log)
target_link_options(game PRIVATE -u ANativeActivity_onCreate)
