cmake_minimum_required(VERSION 3.21)
project(android_pad_host CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
get_filename_component(ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../.. ABSOLUTE)
set(ANDROID_PAD_SOURCE ${ROOT}/platform/android/android_gamepad.cpp CACHE FILEPATH "Gamepad source under test")
add_executable(android_pad_test ${ANDROID_PAD_SOURCE}
  ${ROOT}/platform/android/android_pad_events.cpp ${ROOT}/platform/android/android_gamepad_test.cpp)
target_include_directories(android_pad_test PRIVATE ${JNI_HEADERS} ${JNI_HEADERS}/${JNI_OS} ${ROOT}/platform/android)
# Заголовки NDK разрешают сборку под host libc; стандартные заголовки остаются хостовыми.
target_compile_options(android_pad_test PRIVATE -idirafter ${NDK_HEADERS} -Wall -Wextra -Werror -pedantic)
target_link_libraries(android_pad_test PRIVATE like-nes::engine)
like_nes_add_game(android_pad_test)
set(ANDROID_PAD_DEVICE_SOURCE ${ROOT}/platform/android/android_pad_device.cpp CACHE FILEPATH "JNI device source under test")
add_executable(android_pad_jvm_test ${ANDROID_PAD_DEVICE_SOURCE}
  ${ROOT}/platform/android/android_haptic.cpp
  ${ROOT}/platform/android/android_pad_jvm_test.cpp)
target_include_directories(android_pad_jvm_test PRIVATE ${JNI_HEADERS} ${JNI_HEADERS}/${JNI_OS} ${ROOT}/platform/android)
target_include_directories(android_pad_jvm_test PRIVATE ${ROOT}/engine/platform)
target_compile_options(android_pad_jvm_test PRIVATE -idirafter ${NDK_HEADERS} -Wall -Wextra -Werror -pedantic)
target_link_libraries(android_pad_jvm_test PRIVATE like-nes::engine ${JNI_JVM_LIBRARY})
like_nes_add_game(android_pad_jvm_test)

add_executable(android_lifecycle_test ${ROOT}/platform/android/android_lifecycle.cpp
  ${ROOT}/platform/android/android_lifecycle_test.cpp)
target_include_directories(android_lifecycle_test PRIVATE ${ROOT}/platform/mobile ${ROOT}/platform/android
  ${JNI_HEADERS} ${JNI_HEADERS}/${JNI_OS})
target_include_directories(android_lifecycle_test SYSTEM PRIVATE ${NDK_GLUE})
target_compile_options(android_lifecycle_test PRIVATE -idirafter ${NDK_HEADERS} -Wall -Wextra -Werror -pedantic)
target_link_libraries(android_lifecycle_test PRIVATE like-nes::engine)
like_nes_add_game(android_lifecycle_test)
