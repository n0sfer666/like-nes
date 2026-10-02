# Компонент установки `sdk` (спека #24, В1): заголовки, статические библиотеки Release и Debug,
# GLFW + glfw3webgpu, рантайм wgpu, library.bundle, assetc и like-nesConfig.cmake. Отдельно от
# компонента `engine`: установщики MSI/DMG/AppImage его не видят.
# Сборка префикса — два каталога (Release и Debug) и `cmake --install <каталог> --component sdk`
# из каждого в один префикс; правила библиотек разведены по CONFIGURATIONS.

include(${CMAKE_CURRENT_LIST_DIR}/sdk_headers.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/sdk_links.cmake)
include(CMakePackageConfigHelpers)

set(SDK_INC "include/like-nes")
set(SDK_LIB "lib/like-nes")
set(SDK_CMAKE "lib/cmake/like-nes")

add_custom_target(like_nes_sdk)
add_dependencies(like_nes_sdk ${LIKE_NES_SDK_STATIC})

# Заголовки движка подключают друг друга голым именем, поэтому каждый `#include "x"` в
# поставленном заголовке обязан найтись среди поставленных — иначе игра против префикса не
# соберётся, а в дереве это не видно.
set(sdk_names "")
set(sdk_dirs "")
foreach(rel IN LISTS LIKE_NES_SDK_HEADERS)
  set(src "${CMAKE_SOURCE_DIR}/engine/${rel}")
  if(NOT EXISTS "${src}")
    message(FATAL_ERROR "install_sdk: listed header engine/${rel} does not exist")
  endif()
  get_filename_component(name "${rel}" NAME)
  get_filename_component(dir "${rel}" DIRECTORY)
  list(APPEND sdk_names "${name}")
  list(APPEND sdk_dirs "${dir}")
  install(FILES "${src}" DESTINATION "${SDK_INC}/${dir}" COMPONENT sdk)
endforeach()
list(REMOVE_DUPLICATES sdk_dirs)
set(sdk_third_party webgpu/webgpu.h webgpu/wgpu.h GLFW/glfw3.h GLFW/glfw3native.h glfw3webgpu.h)
foreach(rel IN LISTS LIKE_NES_SDK_HEADERS)
  file(STRINGS "${CMAKE_SOURCE_DIR}/engine/${rel}" lines REGEX "^[ \t]*#[ \t]*include")
  foreach(line IN LISTS lines)
    if(line MATCHES "\"([^\"]+)\"" AND NOT CMAKE_MATCH_1 IN_LIST sdk_names)
      message(FATAL_ERROR "install_sdk: engine/${rel} includes \"${CMAKE_MATCH_1}\", "
                          "which is not a listed SDK header")
    endif()
    # Угловой include — поставленный сторонний либо стандартный заголовок C++ (без точки и
    # слэша). `<stb_image.h>` или `<windows.h>` в публичном заголовке — решение, а не случайность:
    # дописывается сюда вместе с тем, что делает его доступным игре.
    if(line MATCHES "<([^>]+)>")
      set(angle "${CMAKE_MATCH_1}")
      if(NOT angle IN_LIST sdk_third_party AND NOT angle MATCHES "^[a-z_]+$")
        message(FATAL_ERROR "install_sdk: engine/${rel} includes <${angle}>, "
                            "which the SDK does not ship")
      endif()
    endif()
  endforeach()
endforeach()

# Сторонние заголовки — под third_party/, чтобы `<webgpu/webgpu.h>` и `<GLFW/glfw3.h>` нашлись
# тем же путём, что в дереве.
get_target_property(wgpu_inc webgpu INTERFACE_INCLUDE_DIRECTORIES)
install(FILES "${wgpu_inc}/webgpu/webgpu.h" "${wgpu_inc}/webgpu/wgpu.h"
        DESTINATION "${SDK_INC}/third_party/webgpu" COMPONENT sdk)
install(FILES "${glfw_SOURCE_DIR}/include/GLFW/glfw3.h" "${glfw_SOURCE_DIR}/include/GLFW/glfw3native.h"
        DESTINATION "${SDK_INC}/third_party/GLFW" COMPONENT sdk)
install(FILES "${glfw3webgpu_SOURCE_DIR}/glfw3webgpu.h"
        DESTINATION "${SDK_INC}/third_party" COMPONENT sdk)

# Имя файла фиксируется RENAME-ом: Config знает его заранее, а OUTPUT_NAME чужой цели (glfw3)
# не просачивается в контракт префикса.
foreach(tgt IN LISTS LIKE_NES_SDK_STATIC)
  like_nes_sdk_static_name(${tgt} file)
  install(FILES "$<TARGET_FILE:${tgt}>" DESTINATION "${SDK_LIB}" RENAME "${file}"
          CONFIGURATIONS Release COMPONENT sdk)
  install(FILES "$<TARGET_FILE:${tgt}>" DESTINATION "${SDK_LIB}/debug" RENAME "${file}"
          CONFIGURATIONS Debug COMPONENT sdk)
endforeach()

# Рантайм wgpu — в bin/: рядом с assetc, которому rpath @executable_path/$ORIGIN ищет его там же.
# PROGRAMS, а не FILES: на Linux/macOS загрузчику нужен исполняемый бит.
install(PROGRAMS "${WGPU_RUNTIME_LIB}" DESTINATION bin COMPONENT sdk)
if(WIN32)
  install(FILES "${WGPU_RUNTIME_LIB}.lib" DESTINATION "${SDK_LIB}" COMPONENT sdk)
endif()
install(TARGETS assetc RUNTIME DESTINATION bin CONFIGURATIONS Release COMPONENT sdk)
install(FILES "${CMAKE_SOURCE_DIR}/example_ugly_game/assets/library.bundle"
        DESTINATION share/like-nes COMPONENT sdk)

# stb в списке, хотя не поставляется: render_core отдаёт его каталог PUBLIC ради своего TU, а
# проверка выше гарантирует, что ни один поставленный заголовок stb не подключает.
set(LIKE_NES_SDK_INC_ALLOWED "${glfw_SOURCE_DIR}/include" "${glfw3webgpu_SOURCE_DIR}" "${stb_SOURCE_DIR}")
foreach(dir IN LISTS sdk_dirs)
  list(APPEND LIKE_NES_SDK_INC_ALLOWED "${CMAKE_SOURCE_DIR}/engine/${dir}")
endforeach()
list(TRANSFORM LIKE_NES_SDK_INC_ALLOWED REPLACE "/\\.$" "")

set(sdk_includes "")
foreach(dir IN LISTS sdk_dirs)
  list(APPEND sdk_includes "${SDK_INC}/${dir}")
endforeach()
list(APPEND sdk_includes "${SDK_INC}/third_party")
like_nes_sdk_config_data("${sdk_includes}" LIKE_NES_SDK_DATA)
configure_file("${CMAKE_CURRENT_LIST_DIR}/like-nesConfig.cmake.in"
               "${CMAKE_BINARY_DIR}/sdk/like-nesConfig.cmake" @ONLY)

# Пока мажор 0, API ломается между минорами — SameMinorVersion; суффикс -dev в версию пакета не
# идёт, CMake сравнивает только числа.
if(NOT GAME_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
  message(FATAL_ERROR "GAME_VERSION '${GAME_VERSION}' does not start with X.Y.Z or vX.Y.Z")
endif()
set(sdk_version "${CMAKE_MATCH_1}")
write_basic_package_version_file("${CMAKE_BINARY_DIR}/sdk/like-nesConfigVersion.cmake"
  VERSION "${sdk_version}" COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_BINARY_DIR}/sdk/like-nesConfig.cmake"
              "${CMAKE_BINARY_DIR}/sdk/like-nesConfigVersion.cmake"
              "${CMAKE_CURRENT_LIST_DIR}/like_nes_bake.cmake"
        DESTINATION "${SDK_CMAKE}" COMPONENT sdk)
