cmake_minimum_required(VERSION 3.24)
project(like_nes_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)

set(POC ${CMAKE_CURRENT_SOURCE_DIR}/..)

include(${POC}/mobile/wgpu_native.cmake)

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
  ${POC}/mobile/mobile_game.cpp
  ${POC}/game/art.cpp ${POC}/game/batch.cpp ${POC}/game/sim.cpp ${POC}/game/combat.cpp ${POC}/game/boss.cpp
  ${POC}/game/controls.cpp ${POC}/game/draw.cpp ${POC}/game/fx.cpp ${POC}/game/bloom.cpp ${POC}/game/audio.cpp
  ${POC}/input/action_map.cpp ${POC}/render/gpu.cpp)

target_include_directories(game PRIVATE
  ${POC}/game ${POC}/mobile ${POC}/src ${POC}/input ${POC}/render)
target_link_libraries(game PRIVATE
  native_app_glue wgpu_native flecs_static android log)
target_link_options(game PRIVATE -u ANativeActivity_onCreate)
