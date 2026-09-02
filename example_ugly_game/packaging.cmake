# Упаковка и установка образца — свой файл, а не хвост этого. Разрез по границе смысла: выше
# объявляются ЦЕЛИ и их зависимости, здесь — что из построенного едет в бандл каждой ОС, и общего у
# половин ровно имя цели. `include`, а не `add_subdirectory`: область та же, поэтому свойства целей
# и переменные видны как были, а несущий порядок этого каталога (`if(NOT IDE_POC) return()` выше по
# файлу) остаётся ровно тем же — блок исполняется на своём прежнем месте.
# --- Packaging per-OS (спека #8, гейт 4): самодостаточный бандл + version-stamp (git-hash).
# macOS .app (@rpath→Frameworks) / Linux tarball ($ORIGIN, flat) / Windows папка (.exe+DLL).
# game.bundle (baked ассеты, гейт 2) кладётся в бандл → игра стартует с бейкнутых, не с source.
# Сборка бандла: cmake --install <build> --prefix <out> (см. scripts/package.sh).
execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
  OUTPUT_VARIABLE GAME_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GAME_GIT_HASH)
  set(GAME_GIT_HASH "unknown")
endif()
# Версия из тега (release.yml: -DGAME_VERSION=vX.Y.Z), иначе dev-дефолт.
if(NOT GAME_VERSION)
  set(GAME_VERSION "0.1.0-dev")
endif()
set(GAME_TARGET_TRIPLE "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
configure_file(packaging/version.txt.in ${CMAKE_BINARY_DIR}/version.txt @ONLY)
set(GAME_BUNDLE_ASSET ${CMAKE_CURRENT_SOURCE_DIR}/assets/game.bundle)
# Библиотека эффектов едет ОТДЕЛЬНЫМ бандлом, рядом с audio.bundle: перепечь `game.bundle` целиком
# негде, кроме машины владельца (текстурной секции нужны tint и basisu), а материал обязан
# перепекаться правкой текста. В установленный набор он попадает наравне с остальными — игра,
# нашедшая эффекты только в дереве исходников, работает ровно до первой установленной сборки.
set(GAME_LIBRARY_ASSET ${CMAKE_CURRENT_SOURCE_DIR}/assets/library.bundle)

include(${CMAKE_SOURCE_DIR}/cmake/licenses.cmake)

if(APPLE)
  set_target_properties(game_sidescroller PROPERTIES
    INSTALL_RPATH "@executable_path;@executable_path/../Frameworks")
  configure_file(packaging/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)
  set(_app "like-nes.app/Contents")
  install(TARGETS game_sidescroller RUNTIME DESTINATION ${_app}/MacOS COMPONENT game)
  install(FILES ${WGPU_RUNTIME_LIB} DESTINATION ${_app}/Frameworks COMPONENT game)
  install(FILES ${GAME_BUNDLE_ASSET} ${GAME_LIBRARY_ASSET} ${CMAKE_BINARY_DIR}/version.txt
          DESTINATION ${_app}/Resources COMPONENT game)
  install(FILES ${LICENSE_FILES} DESTINATION ${_app}/Resources/licenses COMPONENT game)
  install(FILES ${CMAKE_BINARY_DIR}/Info.plist DESTINATION ${_app} COMPONENT game)
else()
  install(TARGETS game_sidescroller RUNTIME DESTINATION like-nes COMPONENT game)
  install(FILES ${WGPU_RUNTIME_LIB} ${GAME_BUNDLE_ASSET} ${GAME_LIBRARY_ASSET}
          ${CMAKE_BINARY_DIR}/version.txt DESTINATION like-nes COMPONENT game)
  install(FILES ${LICENSE_FILES} DESTINATION like-nes/licenses COMPONENT game)
endif()
