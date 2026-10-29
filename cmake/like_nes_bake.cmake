# like_nes_bake(<цель> MANIFEST <файл> OUT <бандл> [TOOL <цель|путь>]) — бейк ассетов игры
# поставленным assetc (спека #24, В4) или инструментом игры с её видами записей (спека #29, В2а).
# Бандл печётся в `like_nes_bake/` каталога сборки, а рядом с exe его кладёт цель `<цель>_bundle`
# вместе с library.bundle движка; в мобильном приложении оба едут в `assets/`.

# assetc пишет в depfile абсолютные пути, и их все генераторы читают одинаково; NEW — ради одного
# поведения у любой игры, а не того, что задал её cmake_minimum_required (у < 3.20 — OLD и
# предупреждение). Функции запоминают политики в момент определения, поэтому ставится здесь.
cmake_policy(SET CMP0116 NEW)

function(like_nes_bake target)
  cmake_parse_arguments(PARSE_ARGV 1 _lnb "" "MANIFEST;OUT;TOOL" "")
  if(NOT TARGET ${target})
    message(FATAL_ERROR "like_nes_bake: '${target}' is not a target")
  endif()
  if(_lnb_UNPARSED_ARGUMENTS OR NOT _lnb_MANIFEST OR NOT _lnb_OUT)
    message(FATAL_ERROR "like_nes_bake: usage like_nes_bake(<target> MANIFEST <file> OUT <bundle> [TOOL <tool>])")
  endif()
  if(_lnb_OUT MATCHES "[/\\]")
    message(FATAL_ERROR "like_nes_bake: OUT '${_lnb_OUT}' must be a file name; it lands next to the executable")
  endif()
  get_filename_component(manifest "${_lnb_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "like_nes_bake: manifest not found: ${manifest}")
  endif()
  # Инструмент игры — цель этой же сборки или готовый бинарь хоста (мобильная сборка не может
  # запустить то, что собирает под телефон). Путь — абсолютный и существующий: относительный
  # разрешился бы от каталога сборки, и опечатка всплыла бы посреди сборки, а не здесь. Пустой
  # или NOTFOUND TOOL молча откатился бы на assetc SDK, и отказ вида пришёл бы со сборки. Ключ
  # ищется в ARGN: `TOOL ""` cmake_parse_arguments не кладёт в KEYWORDS_MISSING_VALUES ни при какой
  # CMP0174, а `if(NOT …)` ложен и для пустого, и для `*-NOTFOUND`.
  if("TOOL" IN_LIST ARGN AND NOT _lnb_TOOL)
    message(FATAL_ERROR "like_nes_bake: TOOL needs a target or an absolute path, got '${_lnb_TOOL}'")
  endif()
  if(_lnb_TOOL AND TARGET ${_lnb_TOOL})
    set(tool "$<TARGET_FILE:${_lnb_TOOL}>")
    set(tool_dep ${_lnb_TOOL})
  elseif(_lnb_TOOL)
    if(NOT IS_ABSOLUTE "${_lnb_TOOL}" OR NOT EXISTS "${_lnb_TOOL}" OR IS_DIRECTORY "${_lnb_TOOL}")
      message(FATAL_ERROR "like_nes_bake: TOOL '${_lnb_TOOL}' is neither a target nor an existing absolute path")
    endif()
    # Бинарь, собранный против префикса другой сборки, печёт бандл чужим ядром; штамп рядом с ним
    # пишет его проект — та же сверка _ln_build, что у assetc хост-префикса.
    if(NOT EXISTS "${_lnb_TOOL}.like-nes-build")
      message(FATAL_ERROR "like_nes_bake: TOOL '${_lnb_TOOL}' has no ${_lnb_TOOL}.like-nes-build stamp; "
                          "build it against the desktop prefix of the same tree")
    endif()
    file(STRINGS "${_lnb_TOOL}.like-nes-build" stamp LIMIT_COUNT 1)
    if(NOT "${stamp}" STREQUAL "${_ln_build}")
      message(FATAL_ERROR "like_nes_bake: TOOL '${_lnb_TOOL}' was built against like-nes build '${stamp}', "
                          "not ${_ln_build} of this SDK; rebuild it against the desktop prefix of the same tree")
    endif()
    set(tool "${_lnb_TOOL}")
    set(tool_dep "${_lnb_TOOL}")
  else()
    if(NOT LIKE_NES_ASSETC)
      message(FATAL_ERROR "like_nes_bake: a mobile SDK bakes with the desktop SDK's assetc; "
                          "pass -DLIKE_NES_HOST_PREFIX=<desktop like-nes prefix>")
    endif()
    if(NOT EXISTS "${LIKE_NES_ASSETC}")
      message(FATAL_ERROR "like_nes_bake: SDK prefix has no ${LIKE_NES_ASSETC}")
    endif()
    set(tool "${LIKE_NES_ASSETC}")
    set(tool_dep "${LIKE_NES_ASSETC}")
  endif()
  if(NOT EXISTS "${LIKE_NES_LIBRARY_BUNDLE}")
    message(FATAL_ERROR "like_nes_bake: SDK prefix has no ${LIKE_NES_LIBRARY_BUNDLE}")
  endif()

  # Свой подкаталог, а не каталог exe: у одноконфигурационного генератора они совпали бы, копия
  # ниже стала бы копией файла в себя, и гейт SDK на Ninja не судил бы шаг, без которого у Visual
  # Studio и Xcode бандла рядом с exe нет. Подкаталог цели — чтобы игра и, скажем, её редактор с
  # одним OUT в одном каталоге не дали два правила на один выход.
  set(bake_dir "${CMAKE_CURRENT_BINARY_DIR}/like_nes_bake/${target}")
  file(MAKE_DIRECTORY "${bake_dir}")
  set(bundle "${bake_dir}/${_lnb_OUT}")
  # PNG и прочие исходники приходят только через depfile: в DEPENDS их не перечислить, их знает
  # лишь манифест. Сам инструмент — в DEPENDS, иначе новый SDK не перепёк бы старый бандл.
  add_custom_command(OUTPUT "${bundle}"
    COMMAND "${tool}" --manifest "${manifest}" "${bundle}" --depfile "${bundle}.d"
    DEPENDS "${manifest}" ${tool_dep}
    DEPFILE "${bundle}.d"
    COMMENT "Baking ${_lnb_OUT} from ${_lnb_MANIFEST}"
    VERBATIM)
  # В мобильном приложении бандлы едут в `assets/` `.app` или APK, а не рядом с exe.
  if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
    like_nes_mobile_assets(${target} "${bundle}" "${LIKE_NES_LIBRARY_BUNDLE}")
    return()
  endif()
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
