# Десктопная часть компонента `sdk`: GLFW + glfw3webgpu под like-nes::window, рантайм wgpu рядом с
# assetc, implib на Windows. Включается из install_sdk.cmake.

list(APPEND sdk_third_party GLFW/glfw3.h GLFW/glfw3native.h glfw3webgpu.h)
install(FILES "${glfw_SOURCE_DIR}/include/GLFW/glfw3.h" "${glfw_SOURCE_DIR}/include/GLFW/glfw3native.h"
        DESTINATION "${SDK_INC}/third_party/GLFW" COMPONENT sdk)
install(FILES "${glfw3webgpu_SOURCE_DIR}/glfw3webgpu.h"
        DESTINATION "${SDK_INC}/third_party" COMPONENT sdk)

# Рантайм wgpu — в bin/: рядом с assetc, которому rpath @executable_path/$ORIGIN ищет его там же.
# PROGRAMS, а не FILES: на Linux/macOS загрузчику нужен исполняемый бит.
install(PROGRAMS "${WGPU_RUNTIME_LIB}" DESTINATION bin COMPONENT sdk)
if(WIN32)
  install(FILES "${WGPU_RUNTIME_LIB}.lib" DESTINATION "${SDK_LIB}" COMPONENT sdk)
endif()
install(TARGETS assetc RUNTIME DESTINATION bin CONFIGURATIONS Release COMPONENT sdk)

set(LIKE_NES_SDK_INC_ALLOWED "${glfw_SOURCE_DIR}/include" "${glfw3webgpu_SOURCE_DIR}")
set(LIKE_NES_SDK_WEBGPU_FILE "${WGPU_RUNTIME_LIB}")
set(LIKE_NES_SDK_EXTRA_DATA "")
install(FILES "${CMAKE_CURRENT_LIST_DIR}/sdk_config_desktop.cmake" DESTINATION "${SDK_CMAKE}" COMPONENT sdk)
