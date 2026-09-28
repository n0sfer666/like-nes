# Происхождение предсобранного wgpu-native (аудит #21 A·3·2). Блоб приезжает из личного репозитория
# eliemichel/WebGPU-distribution: git-пин по SHA даёт целостность, но не говорит, чем эти байты
# были собраны, а пакет ставит их пользователю рядом с редактором. Суммы в wgpu_native.sha256 сняты
# 2026-09-25 с артефактов релиза gfx-rs/wgpu-native v0.19.4.1 (`wgpu-<тройка>-release.zip`) и
# совпали с блобами форка побайтно на всех шести файлах. Сверяется конечный файл, а не способ его
# доставки — поэтому рукописный `git fetch` транзитивного webgpu.cmake (A·3·15) для
# происхождения больше не важен.
#
# Ключ — `<каталог>/<файл>`, ровно как их раскладывает WebGPU-distribution (`bin/macos-aarch64/…`).
# Файла нет в списке — отказ, а не пропуск: новая тройка без суммы и есть непроверенный блоб.
set(LIKE_NES_WGPU_SUMS ${CMAKE_CURRENT_LIST_DIR}/wgpu_native.sha256)

function(like_nes_wgpu_verify file sums)
  get_filename_component(name "${file}" NAME)
  get_filename_component(dir "${file}" DIRECTORY)
  get_filename_component(dir "${dir}" NAME)
  set(key "${dir}/${name}")
  file(STRINGS "${sums}" lines)
  set(want "")
  foreach(line IN LISTS lines)
    if(line MATCHES "^([0-9a-f]+)  (.+)$" AND CMAKE_MATCH_2 STREQUAL key)
      set(want ${CMAKE_MATCH_1})
    endif()
  endforeach()
  if(NOT want)
    message(FATAL_ERROR "wgpu-native: ${key} не пиннут в ${sums} — непроверенный блоб в пакет не идёт")
  endif()
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "wgpu-native: нет файла ${file}")
  endif()
  file(SHA256 "${file}" got)
  if(NOT got STREQUAL want)
    message(FATAL_ERROR "wgpu-native: sha256 ${key} — ${got}, пиннут ${want} (релиз gfx-rs/wgpu-native v0.19.4.1)")
  endif()
endfunction()
