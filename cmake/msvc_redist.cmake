# App-local CRT: та часть VC++ Redistributable, которую просит ЧУЖОЙ бинарь (спека #20, вертикаль 5).
#
# Статический CRT (cmake/msvc_runtime.cmake) снимает зависимость с наших целей, но в пакете едет
# ещё и предсобранный `wgpu_native.dll` из WebGPU-distribution: он собран чужим тулчейном с
# динамическим CRT и импортирует `VCRUNTIME140.dll`. Пересобрать его нечем — дистрибуция отдаёт
# готовый бинарь, и его пин на коммит есть условие байт-детерминизма бейка.
#
# Поэтому недостающая DLL кладётся РЯДОМ с ним — «local deployment» в терминах Microsoft:
# загрузчик ищет её в каталоге приложения раньше системных, и человеку не нужно ставить ничего.
# UCRT (`api-ms-win-crt-*`, `ucrtbase.dll`) сюда не входит: с Windows 10 это компонент системы.
#
# Список файлов ВЫВОДИТСЯ из таблицы импортов самого рантайма, а не пишется руками: второй список,
# который никто не сверяет с первым, разъезжается молча (тот же класс, что правило list-drift в
# ci_lint.py). Сменится дистрибуция — сменится и набор, без правки этого файла.
if(NOT MSVC)
  set(LIKE_NES_APP_LOCAL_CRT "")
  return()
endif()

# Отказ здесь ЗАВИСИТ от того, релиз ли это. Предмет вертикали — состав РЕЛИЗНОГО пакета, поэтому
# неизвестный app-local CRT валит только релизную конфигурацию; dev-сборка, шаг Build в CI и
# коммит-гейт build_check.sh на раннере Windows идут дальше с пустым списком. Иначе неполная
# установка Visual Studio меняла бы условие коммит-гейта на единственной ОС, где владелец не может
# это проверить, — и по причине, к нашему коду не относящейся.
macro(like_nes_crt_giveup reason)
  if(LIKE_NES_RELEASE)
    message(FATAL_ERROR "spec #20: ${reason}")
  endif()
  message(STATUS "spec #20: ${reason} — app-local CRT не считается (не релизная сборка)")
  set(LIKE_NES_APP_LOCAL_CRT "")
  return()
endmacro()

# Модуль нужен ровно ради `MSVC_REDIST_DIR` — своего `install()` он делать не должен, и своих
# предупреждений тоже. Гасятся ОБЕ половины: `…_SKIP` снимает установку, `…_NO_WARNINGS` —
# `message(WARNING "system runtime library file does not exist")` при неполной установке VS,
# которое иначе красит коммит-гейт (build_check.sh грепает лог на `cmake warning`). Переменные
# ДИРЕКТОРНОЙ области, а файл включается на верхнем уровне, поэтому обе снимаются сразу после
# include: иначе флаг молча погасил бы будущий install системного рантайма где угодно ниже.
#
# Прежние значения ВОССТАНАВЛИВАЮТСЯ, а не стираются: `unset` уничтожил бы осознанно выставленный
# выше флаг — то есть починка одного тихого эффекта вводила бы второй.
set(_crt_skip_was "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP}")
set(_crt_warn_was "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS}")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
include(InstallRequiredSystemLibraries)
if(_crt_skip_was STREQUAL "")
  unset(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP)
else()
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP "${_crt_skip_was}")
endif()
if(_crt_warn_was STREQUAL "")
  unset(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS)
else()
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS "${_crt_warn_was}")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_Interpreter_FOUND)
  like_nes_crt_giveup("без python3 не прочитать импорты ${WGPU_RUNTIME_LIB}")
endif()

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../scripts/pe_imports.py" "${WGPU_RUNTIME_LIB}"
  OUTPUT_VARIABLE _imports RESULT_VARIABLE _rc OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _rc EQUAL 0)
  like_nes_crt_giveup("таблица импортов ${WGPU_RUNTIME_LIB} не прочитана (код ${_rc})")
endif()

# Каталоги-кандидаты: то, что посчитал модуль CMake, переменная окружения от vcvars и вывод из пути
# компилятора. Три источника, потому что ни один не обязателен: `MSVC_REDIST_DIR` появляется не во
# всех версиях модуля, `VCToolsRedistDir` — только под vcvars, а генератор Visual Studio зовёт cl.exe
# по полному пути и без окружения.
set(_redist_dirs "")
if(MSVC_REDIST_DIR)
  list(APPEND _redist_dirs "${MSVC_REDIST_DIR}")
endif()
if(DEFINED ENV{VCToolsRedistDir})
  file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" _env_redist)
  list(APPEND _redist_dirs "${_env_redist}")
endif()
get_filename_component(_vc "${CMAKE_CXX_COMPILER}" DIRECTORY)
foreach(_up RANGE 5)
  get_filename_component(_vc "${_vc}" DIRECTORY)
endforeach()
file(GLOB _guessed "${_vc}/Redist/MSVC/*")
list(APPEND _redist_dirs ${_guessed})
if(NOT _redist_dirs)
  like_nes_crt_giveup("каталог redist не найден ни модулем CMake, ни vcvars, ни рядом с ${CMAKE_CXX_COMPILER}")
endif()

# Подкаталог архитектуры называется целью сборки, а не хостом, и зашитый `x64` молча промахнулся бы
# на ARM64 или 32-разрядной цели: файла не нашлось бы, а сообщение обвиняло бы отсутствующий redist.
# Источников три, и НИ ОДИН не универсален: `CMAKE_VS_PLATFORM_NAME` заполняет только генератор
# Visual Studio (на Ninja он пуст), `CMAKE_CXX_COMPILER_ARCHITECTURE_ID` — MSVC-специфичный ответ
# самого компилятора, `CMAKE_SYSTEM_PROCESSOR` есть всегда, но пишется как угодно. Неизвестное имя
# уходит в like_nes_crt_giveup со СВОИМ текстом: зашитый `x64` промахнулся бы мимо каталога, а
# сообщение обвиняло бы отсутствующий redist — то есть чинили бы не то.
set(_arch "")
foreach(_cand "${CMAKE_VS_PLATFORM_NAME}" "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}"
              "${CMAKE_SYSTEM_PROCESSOR}")
  if(_cand STREQUAL "")
    continue()
  endif()
  string(TOLOWER "${_cand}" _low_arch)
  if(_low_arch MATCHES "^(win32|x86|i[3-6]86)$")
    set(_arch "x86")
  elseif(_low_arch MATCHES "^(x64|amd64|x86_64)$")
    set(_arch "x64")
  elseif(_low_arch MATCHES "^(arm64|aarch64)$")
    set(_arch "arm64")
  elseif(_low_arch STREQUAL "arm")
    set(_arch "arm")
  endif()
  if(NOT _arch STREQUAL "")
    break()
  endif()
endforeach()
if(_arch STREQUAL "")
  like_nes_crt_giveup(
    "архитектура цели не опознана (VS='${CMAKE_VS_PLATFORM_NAME}', компилятор='${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}', система='${CMAKE_SYSTEM_PROCESSOR}') — подкаталог redist назвать нечем")
endif()

set(LIKE_NES_APP_LOCAL_CRT "")
string(REPLACE "\n" ";" _import_list "${_imports}")
foreach(_dll IN LISTS _import_list)
  string(STRIP "${_dll}" _dll)
  string(TOLOWER "${_dll}" _low)
  # Тот же закрытый список, что у crt_is_redist в scripts/release_crt_lib.sh. Копия здесь не делает
  # его источником правды: расхождение двух копий ловит assert_redist_list_mirrored, а недостающий
  # файл в пакете — гейт, читающий импорты уже у ГОТОВОГО пакета.
  if(NOT _low MATCHES "^(vcruntime|msvcp|msvcr|concrt|vccorlib|mfc|vcamp|vcomp)[0-9]")
    continue()
  endif()
  set(_found "")
  foreach(_dir IN LISTS _redist_dirs)
    file(GLOB _hit "${_dir}/${_arch}/Microsoft.VC*.CRT/${_low}")
    if(_hit)
      list(GET _hit 0 _found)
      break()
    endif()
  endforeach()
  if(NOT _found)
    like_nes_crt_giveup(
      "${WGPU_RUNTIME_LIB} импортирует ${_dll}, а файла нет ни в одном каталоге redist для ${_arch} (${_redist_dirs}) — пакет требовал бы VC++ Redistributable")
  endif()
  list(APPEND LIKE_NES_APP_LOCAL_CRT "${_found}")
  message(STATUS "spec #20: app-local CRT — ${_found}")
endforeach()
