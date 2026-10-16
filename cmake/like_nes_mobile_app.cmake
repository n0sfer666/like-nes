# like_nes_mobile_app(<цель> SOURCES <файл>... [BUNDLES <файл>...] NAME <имя> ID <идентификатор>) —
# мобильное приложение на оболочке like_nes_mobile (спека #26, В1): `.app` на iOS, `.so` и цель
# `<цель>_apk` на Android. Одна функция у дерева и у игры против SDK: оболочки дерева — первый её
# потребитель, и упаковка, проверенная на них, та же, что достаётся игре.
# Ждёт LIKE_NES_MOBILE_LIB (цель оболочки), LIKE_NES_MOBILE_SHARE (каталог с ios/ и android/) и
# LIKE_NES_MOBILE_LICENSES — их задают platform/mobile/tree.cmake в дереве и like-nesConfig.cmake
# в префиксе.

include(${CMAKE_CURRENT_LIST_DIR}/sdk_platform.cmake)

# Файлы, которые приложение видит в `assets/`: бандлы игры, движка, файл сверки бенча. На iOS — ресурс
# `.app`, где `resolve_asset` ищет `<exe_dir>/assets/<имя>`; на Android — список для упаковки APK.
function(like_nes_mobile_assets target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "like_nes_mobile_assets: '${target}' is not a target")
  endif()
  # Путь — от каталога вызова: упаковка APK идёт из каталога сборки, и относительный там не нашёлся
  # бы. Одно имя — один файл в `assets/`: второй молча затёр бы первый, а тот же путь повторно
  # (library.bundle от каждого like_nes_bake) — не новый файл.
  get_target_property(have ${target} LIKE_NES_ASSETS)
  if(NOT have)
    set(have "")
  endif()
  set(added "")
  foreach(file IN LISTS ARGN)
    get_filename_component(file "${file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(file IN_LIST have)
      continue()
    endif()
    get_filename_component(name "${file}" NAME)
    foreach(old IN LISTS have)
      get_filename_component(old_name "${old}" NAME)
      if(old_name STREQUAL name)
        message(FATAL_ERROR "like_nes_mobile_assets: '${file}' and '${old}' both land in assets/${name}")
      endif()
    endforeach()
    list(APPEND have "${file}")
    list(APPEND added "${file}")
  endforeach()
  if(added STREQUAL "")
    return()
  endif()
  set_property(TARGET ${target} PROPERTY LIKE_NES_ASSETS ${have})
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(${target} PRIVATE ${added})
    set_source_files_properties(${added} TARGET_DIRECTORY ${target} PROPERTIES
      MACOSX_PACKAGE_LOCATION assets HEADER_FILE_ONLY ON)
  endif()
endfunction()

function(_like_nes_ios_app target name id)
  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${LIKE_NES_MOBILE_SHARE}/ios/Info.plist.in"
    MACOSX_BUNDLE_EXECUTABLE_NAME ${target}
    MACOSX_BUNDLE_GUI_IDENTIFIER ${id}
    MACOSX_BUNDLE_BUNDLE_NAME ${name}
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${id})
endfunction()

# Упаковка — отдельной целью после линковки: ассеты, испечённые like_nes_bake, приходят свойством
# LIKE_NES_ASSETS, которое дописывается и после вызова этой функции.
function(_like_nes_apk target name id)
  set(LIKE_NES_APP_ID "${id}")
  set(LIKE_NES_APP_NAME "${name}")
  set(LIKE_NES_APP_LIB "${target}")
  set(manifest "${CMAKE_CURRENT_BINARY_DIR}/${target}_apk/AndroidManifest.xml")
  configure_file("${LIKE_NES_MOBILE_SHARE}/android/AndroidManifest.xml.in" "${manifest}" @ONLY)
  # libc++_shared.so едет в APK всегда: его поставку обещают THIRD-PARTY-NOTICES-NDK.txt и судит
  # scripts/xcompile_verify.sh, а игре с ANDROID_STL=c++_shared без него не загрузиться.
  like_nes_android_triple("${ANDROID_ABI}" triple)
  set(cxx "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib/${triple}/libc++_shared.so")
  if(NOT EXISTS "${cxx}")
    message(FATAL_ERROR "like_nes_mobile_app: no ${cxx}; configure with the NDK toolchain file")
  endif()
  set(libs --lib "$<TARGET_FILE:${target}>" --lib "${cxx}")
  set(licenses "")
  foreach(lic IN LISTS LIKE_NES_MOBILE_LICENSES)
    list(APPEND licenses --license "${lic}")
  endforeach()
  set(apk "${CMAKE_CURRENT_BINARY_DIR}/${target}.apk")
  set(assets "$<TARGET_PROPERTY:${target},LIKE_NES_ASSETS>")
  add_custom_command(OUTPUT "${apk}"
    COMMAND bash "${LIKE_NES_MOBILE_SHARE}/android/package_apk.sh" --abi "${ANDROID_ABI}"
            --manifest "${manifest}" ${libs} ${licenses} --out "${apk}" -- "${assets}"
    DEPENDS ${target} "${manifest}" "${LIKE_NES_MOBILE_SHARE}/android/package_apk.sh"
            ${LIKE_NES_MOBILE_LICENSES} "${assets}"
    COMMENT "Packaging ${target}.apk"
    COMMAND_EXPAND_LISTS VERBATIM)
  # В ALL, как `.app` на iOS: голый `cmake --build` даёт игре то, что ставится на телефон.
  add_custom_target(${target}_apk ALL DEPENDS "${apk}")
endfunction()

function(like_nes_mobile_app target)
  cmake_parse_arguments(PARSE_ARGV 1 _lnm "" "NAME;ID" "SOURCES;BUNDLES")
  if(_lnm_UNPARSED_ARGUMENTS OR NOT _lnm_SOURCES OR NOT _lnm_NAME OR NOT _lnm_ID)
    message(FATAL_ERROR "like_nes_mobile_app: usage like_nes_mobile_app(<target> SOURCES <file>... "
                        "[BUNDLES <file>...] NAME <name> ID <bundle id>)")
  endif()
  # ID — пакет Java на Android и bundle id на iOS: дефис первому не годится. Имя уходит в XML
  # манифеста и Info.plist без экранирования. Оба отказа здесь, а не в aapt2 посреди сборки.
  if(NOT _lnm_ID MATCHES "^[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z][A-Za-z0-9_]*)+$")
    message(FATAL_ERROR "like_nes_mobile_app: ID '${_lnm_ID}' is not a dotted name like com.example.game")
  endif()
  if(_lnm_NAME MATCHES "[&<>\"]")
    message(FATAL_ERROR "like_nes_mobile_app: NAME '${_lnm_NAME}' must not contain & < > \"")
  endif()
  foreach(var LIKE_NES_MOBILE_LIB LIKE_NES_MOBILE_SHARE)
    if(NOT ${var})
      message(FATAL_ERROR "like_nes_mobile_app: ${var} is not set; find_package(like-nes) sets it")
    endif()
  endforeach()
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(${target} MACOSX_BUNDLE ${_lnm_SOURCES} ${LIKE_NES_MOBILE_LICENSES})
    set_source_files_properties(${LIKE_NES_MOBILE_LICENSES} TARGET_DIRECTORY ${target} PROPERTIES
      MACOSX_PACKAGE_LOCATION Resources/licenses HEADER_FILE_ONLY ON)
    _like_nes_ios_app(${target} "${_lnm_NAME}" "${_lnm_ID}")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(${target} SHARED ${_lnm_SOURCES})
    # Точки входа живут в статической оболочке, а из .so на них никто не ссылается: без `-u` они
    # выпали бы из линковки, и NativeActivity не нашла бы ANativeActivity_onCreate.
    target_link_options(${target} PRIVATE "SHELL:-u ANativeActivity_onCreate" "SHELL:-u android_main")
    _like_nes_apk(${target} "${_lnm_NAME}" "${_lnm_ID}")
  else()
    message(FATAL_ERROR "like_nes_mobile_app: ${CMAKE_SYSTEM_NAME} is not a mobile platform")
  endif()
  target_link_libraries(${target} PRIVATE ${LIKE_NES_MOBILE_LIB})
  like_nes_mobile_assets(${target} ${_lnm_BUNDLES})
endfunction()
