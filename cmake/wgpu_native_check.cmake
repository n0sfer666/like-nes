# Сверка блобов wgpu-native, которые поставила FetchContent_MakeAvailable(webgpu): до неё
# WGPU_RUNTIME_LIB не существует, после неё блоб уже копируется рядом с целями. На Windows у DLL
# есть импорт-библиотека, и линкуется именно она — сверяются обе.
include(${CMAKE_CURRENT_LIST_DIR}/wgpu_native_pin.cmake)
like_nes_wgpu_verify("${WGPU_RUNTIME_LIB}" "${LIKE_NES_WGPU_SUMS}")
if(WIN32)
  like_nes_wgpu_verify("${WGPU_RUNTIME_LIB}.lib" "${LIKE_NES_WGPU_SUMS}")
endif()
