# Мобильная часть компонента `sdk` (спека #26, В1): оболочка like_nes_mobile вместо окна и
# нативного ввода, wgpu-native статикой, шаблоны упаковки и лицензии для like_nes_mobile_app.
# Префикс — один на платформу (ios-sim, ios-device, android-<abi>); assetc у него нет, бейк идёт
# десктопным SDK машины сборки. Включается из install_sdk.cmake.

list(REMOVE_ITEM LIKE_NES_SDK_STATIC input_native glfw glfw3webgpu)
list(APPEND LIKE_NES_SDK_STATIC like_nes_mobile)
set(LIKE_NES_SDK_INC_ALLOWED "")
set(LIKE_NES_SDK_EXTRA_DATA "")
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  # Заголовок native_app_glue нужен только оболочке, а архив — каждой .so: вход NativeActivity в нём.
  list(APPEND LIKE_NES_SDK_STATIC native_app_glue)
  list(APPEND LIKE_NES_SDK_INC_ALLOWED "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
  file(STRINGS "${CMAKE_ANDROID_NDK}/source.properties" ndk_rev REGEX "^Pkg\\.Revision")
  string(REGEX REPLACE "^Pkg\\.Revision *= *" "" ndk_rev "${ndk_rev}")
  string(APPEND LIKE_NES_SDK_EXTRA_DATA "set(_ln_android_api \"${ANDROID_PLATFORM_LEVEL}\")\n"
                                        "set(_ln_ndk_rev \"${ndk_rev}\")\n")
  install(FILES "${CMAKE_SOURCE_DIR}/platform/android/AndroidManifest.xml.in"
                "${CMAKE_SOURCE_DIR}/platform/android/package_apk.sh"
                "${CMAKE_SOURCE_DIR}/platform/android/android_sdk.sh"
          DESTINATION share/like-nes/platform/android COMPONENT sdk)
else()
  string(APPEND LIKE_NES_SDK_EXTRA_DATA "set(_ln_ios_min \"${CMAKE_OSX_DEPLOYMENT_TARGET}\")\n")
  install(FILES "${CMAKE_SOURCE_DIR}/platform/ios/Info.plist.in"
          DESTINATION share/like-nes/platform/ios COMPONENT sdk)
endif()
list(APPEND LIKE_NES_SDK_HEADER_PATHS platform/mobile/mobile_app.hpp platform/mobile/mobile_host.hpp)

# Библиотеку собирает cargo, а не цель дерева: зависимость от неё — явная.
add_dependencies(like_nes_sdk wgpu_native_build)
install(FILES "${WGPU_LIB}" DESTINATION "${SDK_LIB}" COMPONENT sdk)
set(LIKE_NES_SDK_WEBGPU_FILE "${WGPU_LIB}")

set(sdk_license_names "")
foreach(lic IN LISTS LICENSE_FILES)
  get_filename_component(name "${lic}" NAME)
  list(APPEND sdk_license_names "${name}")
endforeach()
string(APPEND LIKE_NES_SDK_EXTRA_DATA "set(_ln_licenses \"${sdk_license_names}\")\n")
install(FILES ${LICENSE_FILES} DESTINATION share/like-nes/licenses COMPONENT sdk)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/sdk_config_mobile.cmake"
              "${CMAKE_CURRENT_LIST_DIR}/like_nes_mobile_app.cmake"
        DESTINATION "${SDK_CMAKE}" COMPONENT sdk)
