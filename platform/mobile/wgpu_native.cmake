include(FetchContent)

FetchContent_Declare(wgpu_native_src
  GIT_REPOSITORY https://github.com/gfx-rs/wgpu-native
  GIT_TAG v0.19.4.1
  GIT_SHALLOW TRUE
  GIT_SUBMODULES "ffi/webgpu-headers"
  GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(wgpu_native_src)

# bindgen 0.69 из Cargo.lock тега не видит полей структур под clang 21 (Xcode 27): в AST больше нет
# ElaboratedType, и 72 структуры webgpu.h выходят непрозрачными — 248 ошибок компиляции крейта.
# 0.72.1 это чинит; патч ставится идемпотентно, повторный configure его не дублирует.
set(_wgpu_patch "${CMAKE_CURRENT_LIST_DIR}/wgpu_bindgen.patch")
execute_process(COMMAND git apply --reverse --check "${_wgpu_patch}"
  WORKING_DIRECTORY "${wgpu_native_src_SOURCE_DIR}" RESULT_VARIABLE _wgpu_patched
  OUTPUT_QUIET ERROR_QUIET)
if(NOT _wgpu_patched EQUAL 0)
  execute_process(COMMAND git apply "${_wgpu_patch}"
    WORKING_DIRECTORY "${wgpu_native_src_SOURCE_DIR}" RESULT_VARIABLE _wgpu_apply)
  if(NOT _wgpu_apply EQUAL 0)
    message(FATAL_ERROR "wgpu_native.cmake: ${_wgpu_patch} does not apply to v0.19.4.1")
  endif()
endif()

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

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # rustc и крейт cc берут минимальный iOS из окружения; без него объекты wgpu-native несут
  # умолчание Rust, а не цель, которую корень задал приложению.
  set(WGPU_CARGO_ENV ${CMAKE_COMMAND} -E env
    "IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  if(NOT DEFINED ANDROID_PLATFORM_LEVEL)
    set(ANDROID_PLATFORM_LEVEL 24)
  endif()
  set(_tcbin "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/bin")
  set(_lk "${_tcbin}/aarch64-linux-android${ANDROID_PLATFORM_LEVEL}-clang")
  set(WGPU_CARGO_ENV ${CMAKE_COMMAND} -E env
    "CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=${_lk}"
    "CC_aarch64_linux_android=${_lk}"
    "AR_aarch64_linux_android=${_tcbin}/llvm-ar")
endif()

add_custom_command(
  OUTPUT "${WGPU_LIB}"
  COMMAND ${WGPU_CARGO_ENV} ${CARGO_BIN} build --release --locked --target ${WGPU_RUST_TARGET}
          --manifest-path "${wgpu_native_src_SOURCE_DIR}/Cargo.toml"
  WORKING_DIRECTORY "${wgpu_native_src_SOURCE_DIR}"
  COMMENT "Building wgpu-native from Rust source for ${WGPU_RUST_TARGET}"
  VERBATIM)
add_custom_target(wgpu_native_build DEPENDS "${WGPU_LIB}")
unset(_wgpu_patch)
unset(_wgpu_patched)
unset(_wgpu_apply)
unset(_tcbin)
unset(_lk)

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

# Цели движка линкуют WebGPU под десктопным именем `webgpu` и зовут копирование его рантайма рядом
# с бинарём. На мобиле библиотека статическая и уезжает внутрь приложения линковкой — копировать
# нечего, а имя то же, чтобы граф целей движка не ветвился по платформе.
add_library(webgpu ALIAS wgpu_native)
function(target_copy_webgpu_binaries)
endfunction()
