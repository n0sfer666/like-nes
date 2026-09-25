# Пин wasmtime C-API для WASM-гейта (спека #6, gate #5). До аудита #21 A·3·9 здесь стоял GLOB
# `wasmtime-*-c-api`, и любая распакованная рядом версия молча линковалась в песочницу, на которой
# держится слово «untrusted». Теперь каталог берётся по точному имени из cmake/wasmtime.sha256 и
# только со штампом `.sha256`, который scripts/fetch_wasmtime.sh пишет после сверки архива.
set(LIKE_NES_WASMTIME_SUMS ${CMAKE_CURRENT_LIST_DIR}/wasmtime.sha256)

function(like_nes_wasmtime_triple system processor out)
  string(TOLOWER "${processor}" cpu)
  if(cpu STREQUAL "arm64")
    set(cpu aarch64)
  elseif(cpu STREQUAL "amd64")
    set(cpu x86_64)
  endif()
  if(system STREQUAL "Darwin")
    set(${out} "${cpu}-macos" PARENT_SCOPE)
  elseif(system STREQUAL "Linux")
    set(${out} "${cpu}-linux" PARENT_SCOPE)
  else()
    set(${out} "${cpu}-${system}" PARENT_SCOPE)
  endif()
endfunction()

# Чужая версия в deps/ и пиннутый каталог без сверенного штампа — отказ конфигурации, а не выбор:
# пропуск гейта молчал бы так же, как подмена. Сверенный каталог ДРУГОЙ пиннутой тройки (fetch с
# WASMTIME_TRIPLE под Rosetta или кросс-сборку) не мешает — он просто не этой сборки. Тройка не
# пиннута вовсе (Windows: у wasmtime там .zip) или пиннутого каталога нет — гейт пропущен одной
# строкой, и подсказка про fetch_wasmtime.sh стоит только там, где скрипт её выполнит.
function(like_nes_wasmtime_stamp_ok dir sum out)
  set(stamp "")
  if(EXISTS "${dir}/.sha256")
    file(STRINGS "${dir}/.sha256" stamp LIMIT_COUNT 1)
  endif()
  if(stamp STREQUAL sum)
    set(${out} TRUE PARENT_SCOPE)
  else()
    set(${out} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(like_nes_wasmtime_dir deps triple out)
  set(${out} "" PARENT_SCOPE)
  file(STRINGS "${LIKE_NES_WASMTIME_SUMS}" pins)
  set(want "")
  foreach(line IN LISTS pins)
    if(line MATCHES "^([0-9a-f]+)  (wasmtime-v[0-9.]+-(.+)-c-api)\\.tar\\.xz$")
      set(sum_${CMAKE_MATCH_2} "${CMAKE_MATCH_1}")
      if(CMAKE_MATCH_3 STREQUAL triple)
        set(want "${CMAKE_MATCH_2}")
      endif()
    endif()
  endforeach()
  if(NOT want)
    message(STATUS "PLUGIN_WASM: wasmtime для ${triple} не пиннут — gate #5 пропущен")
    return()
  endif()
  file(GLOB found LIST_DIRECTORIES true RELATIVE "${deps}" "${deps}/wasmtime-*")
  foreach(name IN LISTS found)
    if(NOT IS_DIRECTORY "${deps}/${name}")
      continue()
    endif()
    if(NOT DEFINED sum_${name})
      message(FATAL_ERROR "PLUGIN_WASM: в deps/ лежит ${name}, пиннут только ${want} — "
                          "удалите чужой каталог и возьмите пин: bash scripts/fetch_wasmtime.sh")
    endif()
    like_nes_wasmtime_stamp_ok("${deps}/${name}" "${sum_${name}}" ok)
    if(NOT ok AND (NOT name STREQUAL want OR EXISTS "${deps}/${want}/include/wasmtime.h"))
      message(FATAL_ERROR "PLUGIN_WASM: deps/${name} не сверен с пином (нет штампа .sha256 "
                          "с суммой ${sum_${name}}) — удалите каталог и bash scripts/fetch_wasmtime.sh")
    endif()
  endforeach()
  if(NOT EXISTS "${deps}/${want}/include/wasmtime.h")
    message(STATUS "PLUGIN_WASM: wasmtime C-API не найден в deps/ — gate #5 пропущен "
                   "(bash scripts/fetch_wasmtime.sh)")
    return()
  endif()
  set(${out} "${deps}/${want}" PARENT_SCOPE)
endfunction()
