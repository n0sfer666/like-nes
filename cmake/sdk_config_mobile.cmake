# Мобильная часть like-nesConfig.cmake (спека #26, В1): wgpu-native статикой, оболочка и упаковка
# через like_nes_mobile_app. Включается из Config после общих проверок; отказ — `_ln_fail` и return
# до первой цели.

set(_ln_share "${_ln_prefix}/share/like-nes")
set(_ln_needed "${_ln_lib}/${_ln_runtime}" "${CMAKE_CURRENT_LIST_DIR}/like_nes_mobile_app.cmake")
if(_ln_platform MATCHES "^android-")
  list(APPEND _ln_needed "${_ln_share}/platform/android/AndroidManifest.xml.in"
       "${_ln_share}/platform/android/package_apk.sh" "${_ln_share}/platform/android/android_sdk.sh")
else()
  list(APPEND _ln_needed "${_ln_share}/platform/ios/Info.plist.in")
endif()
set(LIKE_NES_MOBILE_LICENSES "")
foreach(_ln_n IN LISTS _ln_licenses)
  list(APPEND LIKE_NES_MOBILE_LICENSES "${_ln_share}/licenses/${_ln_n}")
endforeach()
foreach(_ln_f IN LISTS _ln_needed LIKE_NES_MOBILE_LICENSES)
  if(NOT EXISTS "${_ln_f}")
    set(_ln_fail "like-nes SDK at ${_ln_prefix} is missing ${_ln_f}; install the sdk component from a Release build")
    return()
  endif()
endforeach()
if(_ln_platform MATCHES "^android-")
  # .so игры и wgpu-native внутри префикса собраны под этот bionic: игра ниже уровнем взяла бы
  # символы, которых на телефоне её минимума нет, выше — обещала бы манифестом меньше, чем требует.
  if(NOT ANDROID_PLATFORM_LEVEL STREQUAL _ln_android_api)
    string(CONCAT _ln_fail "like-nes SDK at ${_ln_prefix} is built for Android API ${_ln_android_api}, this build "
                           "targets '${ANDROID_PLATFORM_LEVEL}'; configure with -DANDROID_PLATFORM=android-${_ln_android_api}")
    return()
  endif()
  # Статика префикса собрана libc++ своего NDK, а libc++_shared.so в APK кладёт NDK игры: чужая
  # ревизия собралась бы и слинковалась, а разошлась бы уже на телефоне.
  set(_ln_rev "")
  if(EXISTS "${CMAKE_ANDROID_NDK}/source.properties")
    file(STRINGS "${CMAKE_ANDROID_NDK}/source.properties" _ln_rev REGEX "^Pkg\\.Revision")
    string(REGEX REPLACE "^Pkg\\.Revision *= *" "" _ln_rev "${_ln_rev}")
  endif()
  if(NOT _ln_rev STREQUAL _ln_ndk_rev)
    string(CONCAT _ln_fail "like-nes SDK at ${_ln_prefix} is built with NDK ${_ln_ndk_rev}, this build uses "
                           "'${_ln_rev}' at '${CMAKE_ANDROID_NDK}'; configure with the toolchain file of NDK ${_ln_ndk_rev}")
    return()
  endif()
# Минимальный iOS задаётся до project(): Config его уже не поставит. Пустой — clang берёт версию
# SDK, а Info.plist получает пустой MinimumOSVersion; другой — обещание, которого префикс не держит.
elseif(NOT CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL _ln_ios_min)
  string(CONCAT _ln_fail "like-nes SDK at ${_ln_prefix} is built for minimum iOS ${_ln_ios_min}, this build "
                         "targets '${CMAKE_OSX_DEPLOYMENT_TARGET}'; configure with -DCMAKE_OSX_DEPLOYMENT_TARGET=${_ln_ios_min}")
  return()
endif()

# assetc — программа машины сборки, а не телефона: его даёт десктопный префикс того же дерева.
# Бандл чужой сборки читатель движка отверг бы только на телефоне, поэтому сборки сверяются здесь.
set(LIKE_NES_HOST_PREFIX "" CACHE PATH "Desktop like-nes SDK prefix whose assetc bakes assets for this mobile build")
set(LIKE_NES_ASSETC "")
if(LIKE_NES_HOST_PREFIX)
  set(_ln_host_cfg "${LIKE_NES_HOST_PREFIX}/lib/cmake/like-nes/like-nesConfig.cmake")
  set(_ln_host "")
  if(EXISTS "${_ln_host_cfg}")
    file(STRINGS "${_ln_host_cfg}" _ln_host REGEX "^set\\(_ln_(platform|build) ")
  endif()
  set(_ln_want "set(_ln_platform \"desktop\")" "set(_ln_build \"${_ln_build}\")")
  if(NOT _ln_host STREQUAL _ln_want OR NOT EXISTS "${LIKE_NES_HOST_PREFIX}/bin/assetc")
    string(CONCAT _ln_fail "LIKE_NES_HOST_PREFIX '${LIKE_NES_HOST_PREFIX}' is not a desktop like-nes SDK of build "
                           "${_ln_build} with bin/assetc; install the desktop sdk component from the same tree")
    return()
  endif()
  set(LIKE_NES_ASSETC "${LIKE_NES_HOST_PREFIX}/bin/assetc")
endif()

if(NOT TARGET like-nes::webgpu)
  add_library(like-nes::webgpu STATIC IMPORTED)
  set_target_properties(like-nes::webgpu PROPERTIES
    IMPORTED_LOCATION "${_ln_lib}/${_ln_runtime}"
    INTERFACE_COMPILE_DEFINITIONS "${_ln_defs_webgpu}")
endif()

set(LIKE_NES_MOBILE_LIB like-nes::like_nes_mobile)
set(LIKE_NES_MOBILE_SHARE "${_ln_share}/platform")
include("${CMAKE_CURRENT_LIST_DIR}/like_nes_mobile_app.cmake")
