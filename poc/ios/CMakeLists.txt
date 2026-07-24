cmake_minimum_required(VERSION 3.24)
project(like_nes_ios LANGUAGES C CXX OBJC OBJCXX)

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

add_executable(like_nes_ios MACOSX_BUNDLE
  ${CMAKE_CURRENT_SOURCE_DIR}/app.mm ${CMAKE_CURRENT_SOURCE_DIR}/view.mm
  ${POC}/game/art.cpp ${POC}/game/batch.cpp ${POC}/game/sim.cpp ${POC}/game/combat.cpp ${POC}/game/boss.cpp
  ${POC}/game/controls.cpp ${POC}/game/draw.cpp
  ${POC}/input/action_map.cpp ${POC}/render/gpu.cpp)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/app.mm ${CMAKE_CURRENT_SOURCE_DIR}/view.mm
  PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

target_include_directories(like_nes_ios PRIVATE
  ${POC}/game ${POC}/src ${POC}/input ${POC}/render ${POC}/ios)
target_link_libraries(like_nes_ios PRIVATE wgpu_native flecs_static
  "-framework UIKit" "-framework Metal" "-framework QuartzCore"
  "-framework Foundation" "-framework CoreGraphics")

set_target_properties(like_nes_ios PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
  MACOSX_BUNDLE_EXECUTABLE_NAME like_nes_ios
  MACOSX_BUNDLE_GUI_IDENTIFIER com.likenes.sidescroller
  MACOSX_BUNDLE_BUNDLE_NAME like-nes)
