# Десктопная часть like-nesConfig.cmake: рантайм wgpu рядом с exe, окно GLFW, assetc префикса.
# Включается из Config после общих проверок; отказ — `_ln_fail` и return до первой цели.

# implib на Windows — тот же класс, что пропавшая библиотека: без него ошибка линкера вместо отказа
# find_package. assetc — тоже: без него like_nes_bake упал бы на первой сборке игры, а не при поиске
# SDK.
set(_ln_assetc "bin/assetc")
if(WIN32)
  string(APPEND _ln_assetc ".exe")
endif()
set(_ln_needed "bin/${_ln_runtime}" "${_ln_assetc}")
if(WIN32)
  list(APPEND _ln_needed "lib/like-nes/${_ln_runtime}.lib")
endif()
foreach(_ln_f IN LISTS _ln_needed)
  if(NOT EXISTS "${_ln_prefix}/${_ln_f}")
    set(_ln_fail "like-nes SDK at ${_ln_prefix} is missing ${_ln_f}; install the sdk component from a Release build")
    return()
  endif()
endforeach()

if(NOT TARGET like-nes::webgpu)
  add_library(like-nes::webgpu SHARED IMPORTED)
  set_target_properties(like-nes::webgpu PROPERTIES
    IMPORTED_LOCATION "${_ln_prefix}/bin/${_ln_runtime}"
    INTERFACE_COMPILE_DEFINITIONS "${_ln_defs_webgpu}")
  if(WIN32)
    set_target_properties(like-nes::webgpu PROPERTIES
      IMPORTED_IMPLIB "${_ln_lib}/${_ln_runtime}.lib")
  elseif(NOT APPLE)
    # Без SONAME у предсобранного .so CMake вшил бы в DT_NEEDED полный путь префикса.
    set_target_properties(like-nes::webgpu PROPERTIES IMPORTED_NO_SONAME TRUE)
  endif()
  add_library(like-nes::window INTERFACE IMPORTED)
  set_target_properties(like-nes::window PROPERTIES
    INTERFACE_LINK_LIBRARIES "like-nes::glfw3webgpu;like-nes::input_native"
    INTERFACE_INCLUDE_DIRECTORIES "${_ln_prefix}/include/like-nes/third_party")
endif()

set(LIKE_NES_RUNTIME "${_ln_prefix}/bin/${_ln_runtime}")
set(LIKE_NES_ASSETC "${_ln_prefix}/${_ln_assetc}")

# Движок собран со статическим CRT (cmake/msvc_runtime.cmake): цель игры обязана взять тот же,
# иначе LNK2038. Рантайм wgpu кладётся рядом с exe, rpath смотрит туда же.
# У multi-config генератора (Visual Studio, Ninja Multi-Config) конфигурация Debug есть всегда,
# и без Debug-набора она молча взяла бы Release-библиотеки — LNK2038 вместо названной причины.
# Зовут игра (like_nes_add_game) и инструменты игры, собранные против префикса.
function(like_nes_require_debug_libs target)
  get_property(_ln_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(MSVC AND NOT LIKE_NES_HAS_DEBUG AND (_ln_multi OR CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message(FATAL_ERROR "${target}: Debug on MSVC needs the Debug SDK libraries "
                        "(lib/like-nes/debug), this prefix has Release only; with a multi-config "
                        "generator use a single-config one (Ninja) and CMAKE_BUILD_TYPE=Release")
  endif()
endfunction()

function(like_nes_add_game target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "like_nes_add_game: '${target}' is not a target")
  endif()
  like_nes_require_debug_libs(${target})
  set_target_properties(${target} PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(APPLE)
    set_target_properties(${target} PROPERTIES BUILD_RPATH "@executable_path")
  elseif(NOT WIN32)
    set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN")
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LIKE_NES_RUNTIME}" "$<TARGET_FILE_DIR:${target}>"
    VERBATIM)
endfunction()
