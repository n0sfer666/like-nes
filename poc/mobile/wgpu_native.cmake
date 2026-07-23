include(FetchContent)

FetchContent_Declare(wgpu_native_src
  GIT_REPOSITORY https://github.com/gfx-rs/wgpu-native
  GIT_TAG v0.19.4.1
  GIT_SHALLOW TRUE
  GIT_SUBMODULES "ffi/webgpu-headers"
  GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(wgpu_native_src)

if(NOT DEFINED WGPU_RUST_TARGET)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(CMAKE_OSX_SYSROOT MATCHES "iPhoneSimulator|iphonesimulator")
      set(WGPU_RUST_TARGET "aarch64-apple-ios-sim")
    else()
      set(WGPU_RUST_TARGET "aarch64-apple-ios")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(WGPU_RUST_TARGET "aarch64-linux-android")
  else()
    message(FATAL_ERROR "wgpu_native.cmake: set WGPU_RUST_TARGET for ${CMAKE_SYSTEM_NAME}")
  endif()
endif()

find_program(CARGO_BIN cargo REQUIRED)
set(WGPU_LIB "${wgpu_native_src_SOURCE_DIR}/target/${WGPU_RUST_TARGET}/release/libwgpu_native.a")

add_custom_command(
  OUTPUT "${WGPU_LIB}"
  COMMAND ${CARGO_BIN} build --release --locked --target ${WGPU_RUST_TARGET}
          --manifest-path "${wgpu_native_src_SOURCE_DIR}/Cargo.toml"
  WORKING_DIRECTORY "${wgpu_native_src_SOURCE_DIR}"
  COMMENT "Building wgpu-native from Rust source for ${WGPU_RUST_TARGET}"
  VERBATIM)
add_custom_target(wgpu_native_build DEPENDS "${WGPU_LIB}")

set(WGPU_SHIM "${wgpu_native_src_SOURCE_DIR}/webgpu_shim")
file(MAKE_DIRECTORY "${WGPU_SHIM}/webgpu")
configure_file("${wgpu_native_src_SOURCE_DIR}/ffi/webgpu-headers/webgpu.h"
               "${WGPU_SHIM}/webgpu/webgpu.h" COPYONLY)
configure_file("${wgpu_native_src_SOURCE_DIR}/ffi/wgpu.h"
               "${WGPU_SHIM}/webgpu/wgpu.h" COPYONLY)

add_library(wgpu_native STATIC IMPORTED GLOBAL)
set_target_properties(wgpu_native PROPERTIES IMPORTED_LOCATION "${WGPU_LIB}")
target_include_directories(wgpu_native INTERFACE "${WGPU_SHIM}")
add_dependencies(wgpu_native wgpu_native_build)
