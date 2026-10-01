# like_nes_bake(<цель> MANIFEST <файл> OUT <бандл>) — бейк ассетов игры поставленным assetc
# (спека #24, В4). Бандл печётся в `like_nes_bake/` каталога сборки, а рядом с exe его кладёт
# цель `<цель>_bundle` вместе с library.bundle движка.

# assetc пишет в depfile абсолютные пути, и их все генераторы читают одинаково; NEW — ради одного
# поведения у любой игры, а не того, что задал её cmake_minimum_required (у < 3.20 — OLD и
# предупреждение). Функции запоминают политики в момент определения, поэтому ставится здесь.
cmake_policy(SET CMP0116 NEW)

function(like_nes_bake target)
  cmake_parse_arguments(PARSE_ARGV 1 _lnb "" "MANIFEST;OUT" "")
  if(NOT TARGET ${target})
    message(FATAL_ERROR "like_nes_bake: '${target}' is not a target")
  endif()
  if(_lnb_UNPARSED_ARGUMENTS OR NOT _lnb_MANIFEST OR NOT _lnb_OUT)
    message(FATAL_ERROR "like_nes_bake: usage like_nes_bake(<target> MANIFEST <file> OUT <bundle>)")
  endif()
  if(_lnb_OUT MATCHES "[/\\]")
    message(FATAL_ERROR "like_nes_bake: OUT '${_lnb_OUT}' must be a file name; it lands next to the executable")
  endif()
  get_filename_component(manifest "${_lnb_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "like_nes_bake: manifest not found: ${manifest}")
  endif()
  foreach(_lnb_need IN ITEMS "${LIKE_NES_LIBRARY_BUNDLE}" "${LIKE_NES_ASSETC}")
    if(NOT EXISTS "${_lnb_need}")
      message(FATAL_ERROR "like_nes_bake: SDK prefix has no ${_lnb_need}")
    endif()
  endforeach()

  # Свой подкаталог, а не каталог exe: у одноконфигурационного генератора они совпали бы, копия
  # ниже стала бы копией файла в себя, и гейт SDK на Ninja не судил бы шаг, без которого у Visual
  # Studio и Xcode бандла рядом с exe нет. Подкаталог цели — чтобы игра и, скажем, её редактор с
  # одним OUT в одном каталоге не дали два правила на один выход.
  set(bake_dir "${CMAKE_CURRENT_BINARY_DIR}/like_nes_bake/${target}")
  file(MAKE_DIRECTORY "${bake_dir}")
  set(bundle "${bake_dir}/${_lnb_OUT}")
  # PNG и прочие исходники приходят только через depfile: в DEPENDS их не перечислить, их знает
  # лишь манифест. Сам assetc — в DEPENDS, иначе новый SDK не перепёк бы старый бандл.
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${LIKE_NES_ASSETC}" --manifest "${manifest}" "${bundle}" --depfile "${bundle}.d"
    DEPENDS "${manifest}" "${LIKE_NES_ASSETC}"
    DEPFILE "${bundle}.d"
    COMMENT "Baking ${_lnb_OUT} from ${_lnb_MANIFEST}"
    VERBATIM)
  # Копия — отдельной всегда исполняемой целью, а не POST_BUILD: тот идёт только при перелинковке
  # exe, и перепечённый после правки PNG бандл не доехал бы до каталога exe.
  add_custom_target(${target}_bundle
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${bundle}" "$<TARGET_FILE_DIR:${target}>/${_lnb_OUT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LIKE_NES_LIBRARY_BUNDLE}"
            "$<TARGET_FILE_DIR:${target}>/library.bundle"
    DEPENDS "${bundle}"
    VERBATIM)
  add_dependencies(${target} ${target}_bundle)
endfunction()
