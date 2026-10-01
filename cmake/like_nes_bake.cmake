# like_nes_bake(<цель> MANIFEST <файл> OUT <бандл>) — сигнатура финальная (спека #24). Тело В1 —
# заглушка: проверяет манифест и кладёт поставленный library.bundle рядом с exe, чтобы путь бандла
# игры был доказан сборкой против префикса. В4 заменит тело на `assetc --manifest` с depfile.

function(like_nes_bake target)
  cmake_parse_arguments(PARSE_ARGV 1 _lnb "" "MANIFEST;OUT" "")
  if(NOT TARGET ${target})
    message(FATAL_ERROR "like_nes_bake: '${target}' is not a target")
  endif()
  if(_lnb_UNPARSED_ARGUMENTS OR NOT _lnb_MANIFEST OR NOT _lnb_OUT)
    message(FATAL_ERROR "like_nes_bake: usage like_nes_bake(<target> MANIFEST <file> OUT <bundle>)")
  endif()
  get_filename_component(manifest "${_lnb_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "like_nes_bake: manifest not found: ${manifest}")
  endif()
  if(NOT EXISTS "${LIKE_NES_LIBRARY_BUNDLE}")
    message(FATAL_ERROR "like_nes_bake: SDK prefix has no ${LIKE_NES_LIBRARY_BUNDLE}")
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LIKE_NES_LIBRARY_BUNDLE}"
            "$<TARGET_FILE_DIR:${target}>/library.bundle"
    VERBATIM)
endfunction()
