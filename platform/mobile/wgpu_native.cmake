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

include(${CMAKE_SOURCE_DIR}/cmake/sdk_platform.cmake)
if(NOT DEFINED WGPU_RUST_TARGET)
  like_nes_sdk_platform(_wgpu_platform)
  if(_wgpu_platform STREQUAL "ios-sim")
    set(WGPU_RUST_TARGET "aarch64-apple-ios-sim")
  elseif(_wgpu_platform STREQUAL "ios-device")
    set(WGPU_RUST_TARGET "aarch64-apple-ios")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    like_nes_android_triple("${ANDROID_ABI}" WGPU_RUST_TARGET)
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
  # Тулчейн NDK без -DANDROID_PLATFORM молча берёт свой минимум (21): .so и wgpu-native собрались
  # бы под bionic ниже, чем обещает манифест. Источник уровня — шаблон манифеста всех приложений.
  file(STRINGS "${CMAKE_SOURCE_DIR}/platform/android/AndroidManifest.xml.in" _min_sdk
       REGEX "android:minSdkVersion=\"[0-9]+\"" LIMIT_COUNT 1)
  string(REGEX REPLACE ".*android:minSdkVersion=\"([0-9]+)\".*" "\\1" _min_sdk "${_min_sdk}")
  if(NOT ANDROID_PLATFORM_LEVEL STREQUAL _min_sdk)
    message(FATAL_ERROR "wgpu_native.cmake: ANDROID_PLATFORM_LEVEL '${ANDROID_PLATFORM_LEVEL}' differs from minSdkVersion '${_min_sdk}' of platform/android/AndroidManifest.xml.in; configure with -DANDROID_PLATFORM=android-${_min_sdk}")
  endif()
  set(_tcbin "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/bin")
  set(_lk "${_tcbin}/${WGPU_RUST_TARGET}${ANDROID_PLATFORM_LEVEL}-clang")
  string(REPLACE "-" "_" _triple_us "${WGPU_RUST_TARGET}")
  string(TOUPPER "${_triple_us}" _triple_up)
  set(WGPU_CARGO_ENV ${CMAKE_COMMAND} -E env
    "CARGO_TARGET_${_triple_up}_LINKER=${_lk}"
    "CC_${_triple_us}=${_lk}"
    "AR_${_triple_us}=${_tcbin}/llvm-ar")
endif()
# Гейт wgpu-pin читает её из кэша: у мобильной сборки нет блоба релиза, есть тег и тройка.
set(WGPU_SOURCE_TARGET "${WGPU_RUST_TARGET}" CACHE INTERNAL "Rust target of the wgpu-native source build")

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
unset(_wgpu_platform)
unset(_tcbin)
unset(_lk)
unset(_triple_us)
unset(_triple_up)
unset(_min_sdk)

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
