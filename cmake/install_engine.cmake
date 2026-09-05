# Пакет ДВИЖКА (спека #20, вертикаль 1) — отдельный компонент установки рядом с `game`. Разрез по
# получателю, а не по каталогу: `game` едет игроку и несёт бандлы уровня, `engine` едет тому, кто
# делает игру, и несёт редактор, пекаря и рантайм. Общий у них только штамп версии и лицензии,
# поэтому и то и другое считается один раз (cmake/version_stamp.cmake, cmake/licenses.cmake).
include(${CMAKE_CURRENT_LIST_DIR}/version_stamp.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/licenses.cmake)

# NEWLINE_STYLE LF — не вкус: штамп читается ЯКОРНЫМИ грепами гейта (`^commit …$`), и CRLF на
# входе шаблона проехал бы в вывод, уронив гейт только на Windows и молча (см. .gitattributes).
configure_file(${CMAKE_CURRENT_LIST_DIR}/engine_version.txt.in
               ${CMAKE_BINARY_DIR}/engine_version.txt @ONLY NEWLINE_STYLE LF)

set(ENGINE_ROOT "like-nes")
option(LIKE_NES_RELEASE "Сборка релизного пакета: неполный состав компонента engine — отказ" OFF)

# Редактор собирается только при PLUGIN_UI=ON (tools/ide: `if(TARGET imgui)`). Каталог, однажды
# сконфигурированный без него, отдал бы `--component engine` МОЛЧА и без редактора — пакет, который
# называет себя движком, а редактора в нём нет. Наполовину заданная конфигурация обязана быть
# отказом, а не тихим уменьшением состава, поэтому релизная сборка требует цель, а не надеется.
if(NOT TARGET editor_shell)
  if(LIKE_NES_RELEASE)
    message(FATAL_ERROR
      "spec #20: сборка релиза без editor_shell — включи PLUGIN_UI=ON, иначе пакет неполон")
  endif()
  message(STATUS "spec #20: компонент engine без редактора (PLUGIN_UI=OFF)")
else()
  install(TARGETS editor_shell RUNTIME DESTINATION ${ENGINE_ROOT}/bin COMPONENT engine)
endif()

install(TARGETS assetc RUNTIME DESTINATION ${ENGINE_ROOT}/bin COMPONENT engine)
# Рантайм wgpu ложится РЯДОМ с исполняемыми, потому что rpath у обеих целей — `@executable_path`
# ($ORIGIN на Linux), см. reproducible_rpath в cmake/determinism.cmake. Иное место потребовало бы
# второго rpath и разошлось бы с тем, как эти же цели находят библиотеку в дереве сборки.
# PROGRAMS, а не FILES: `install(FILES)` ставит 0644, и разделяемая библиотека уезжает к людям без
# бита исполнения. Права идут В АРХИВ, то есть в его сумму, — «переставим потом» тут значит «другой
# пакет».
install(PROGRAMS ${WGPU_RUNTIME_LIB} DESTINATION ${ENGINE_ROOT}/bin COMPONENT engine)
# Рядом с ЧУЖИМ рантаймом ложится та часть CRT, которую просит он сам: наши цели статичны, а его
# пересобрать нечем. Список выводится из его же таблицы импортов — см. cmake/msvc_redist.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/msvc_redist.cmake)
if(LIKE_NES_APP_LOCAL_CRT)
  install(PROGRAMS ${LIKE_NES_APP_LOCAL_CRT} DESTINATION ${ENGINE_ROOT}/bin COMPONENT engine)
endif()
install(FILES ${LICENSE_FILES} DESTINATION ${ENGINE_ROOT}/licenses COMPONENT engine)
install(FILES ${CMAKE_BINARY_DIR}/engine_version.txt
        DESTINATION ${ENGINE_ROOT} RENAME version.txt COMPONENT engine)
