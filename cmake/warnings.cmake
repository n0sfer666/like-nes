# Строгий режим компилятора — аналог тайпчекера с включённым strict: предупреждение = ошибка
# сборки. До этого дерево собиралось на уровне по умолчанию, где -Wall выключен, и варнинг
# «'%s' может обрезать 255 байт» месяцами жил в логе Ubuntu-CI, видимый только глазами.
#
# Файл подключается ПОСЛЕ FetchContent и ДО add_subdirectory нашего кода — в этом вся механика:
# add_compile_options действует на текущий каталог и на подкаталоги, добавленные ПОСЛЕ вызова,
# поэтому строгость достаётся ровно нашему коду, а уже добавленным зависимостям — нет.

option(LIKE_NES_WERROR "Compiler warnings in our own code are build errors" ON)

if(MSVC)
    # /external:W0 — уровень 0 для заголовков, помеченных системными (ниже, like_nes_vendored_headers).
    # /external:templates- обязателен в паре с ним: по умолчанию MSVC ПОКАЗЫВАЕТ диагностику из
    # чужого шаблона, если инстанцирование пришло из нашего TU, а именно так она и приходит —
    # flecs весь на шаблонах, и component.hpp отдаёт C4127 «условное выражение является
    # константой» с цепочкой инстанцирования, начинающейся в нашем scene.cpp.
    # `/external:templates-` — только настоящему cl.exe. clang-cl (им собран ASan-гейт Windows) флаг
    # не реализует и под `-Werror` валит сборку на `argument unused during compilation`; ему он и не
    # нужен: clang не показывает диагностику из системных заголовков, включая инстанцирование
    # шаблона из нашего TU. `MSVC` истинен для обоих, различает их только COMPILER_ID.
    set(LIKE_NES_WARN_FLAGS /W4 /external:W0)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        list(APPEND LIKE_NES_WARN_FLAGS /external:templates-)
    endif()
    set(LIKE_NES_WARN_ERROR /WX)
    set(LIKE_NES_NO_WARN /w)
else()
    set(LIKE_NES_WARN_FLAGS -Wall -Wextra)
    set(LIKE_NES_WARN_ERROR -Werror)
    set(LIKE_NES_NO_WARN -w)
endif()

# Свой уровень предупреждений зависимости приходится СНИМАТЬ, а не перекрывать: glfw ставит себе
# `/W3`, и приписанный следом `/w` даёт `D9025 overriding '/W3'` на каждый объект — предупреждение
# драйвера, которое роняет сборочный гейт. Снимается он с двух свойств цели: COMPILE_OPTIONS —
# так делает glfw, единственный сегодняшний источник, — и устаревшей строки COMPILE_FLAGS, которую
# в текущем наборе зависимостей не ставит никто; ветка держится потому, что её отсутствие
# обнаружилось бы красной сборкой MSVC, то есть кругом CI ценой в двадцать минут, а стоит она
# четыре строки. Каталожный `add_compile_options` третьим случаем не является — CMake копирует
# свойство каталога в цель в момент её создания, то есть флаг приезжает в тот же COMPILE_OPTIONS.
# Регулярка ищет флаг как отдельный токен, а не всю строку целиком: он бывает завёрнут в
# генвыражение `$<$<COMPILE_LANGUAGE:C>:/W3>`. Из CMAKE_<LANG>_FLAGS уровень не берётся —
# CMP0092 в NEW при cmake_minimum_required 3.24.
set(LIKE_NES_WARN_LEVEL_RE "(^|[^A-Za-z0-9_])[-/]W(all|[0-4])($|[^A-Za-z0-9_])")

# Сторонний код чужой: править его нельзя (пин на коммит — условие байт-детерминизма бейка),
# а его варнинги в логе неотличимы от наших. Глушим по факту принадлежности к дереву
# зависимостей, а не списком имён: список устареет на первой же новой зависимости.
function(like_nes_strip_warning_level target)
    get_target_property(opts ${target} COMPILE_OPTIONS)
    if(opts)
        list(FILTER opts EXCLUDE REGEX "${LIKE_NES_WARN_LEVEL_RE}")
        set_property(TARGET ${target} PROPERTY COMPILE_OPTIONS ${opts})
    endif()
    get_target_property(flags ${target} COMPILE_FLAGS)
    if(flags)
        string(REGEX REPLACE "${LIKE_NES_WARN_LEVEL_RE}" " " flags "${flags}")
        set_property(TARGET ${target} PROPERTY COMPILE_FLAGS "${flags}")
    endif()
endfunction()

# Снять уровень с ЦЕЛИ зависимости мало: её заголовки компилируются в НАШЕЙ цели, с нашим /W4, и
# чужая диагностика приезжает как ошибка сборки нашего файла. Так дерево и встало на flecs: он
# header-only для потребителя, и C4127 из component.hpp падал на tools/ide/scene.cpp. Пометка
# системными переводит их на -isystem (/external:I у MSVC), после чего уровень к ним не
# применяется. Правится общим свойством, а не списком путей: список устареет на первой же
# зависимости, добавленной кем-то другим.
function(like_nes_vendored_headers target)
    get_target_property(inc ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(inc)
        set_property(TARGET ${target} APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${inc})
    endif()
endfunction()

function(like_nes_silence_dependencies dir)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t IN LISTS targets)
        get_target_property(type ${t} TYPE)
        # Заголовки есть и у INTERFACE-целей — они как раз ничего, кроме заголовков, и не отдают.
        if(NOT type STREQUAL "UTILITY")
            like_nes_vendored_headers(${t})
        endif()
        # У INTERFACE-целей и custom-таргетов нет своей компиляции — set_property на них
        # либо бессмыслен, либо ошибка конфигурации.
        if(NOT type STREQUAL "INTERFACE_LIBRARY" AND NOT type STREQUAL "UTILITY")
            like_nes_strip_warning_level(${t})
            set_property(TARGET ${t} APPEND PROPERTY COMPILE_OPTIONS ${LIKE_NES_NO_WARN})
        endif()
    endforeach()
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(s IN LISTS subdirs)
        like_nes_silence_dependencies("${s}")
    endforeach()
endfunction()

# Цель целиком из стороннего кода. Снимается ровно УРОВЕНЬ; `-Wextra`/`-Werror` (`/WX`) остаются
# на цели и гасятся тем, что `-w` дописан последним. Держать их снятие отдельной задачей незачем:
# `D9025` MSVC выдаёт именно на смену уровня, а `/WX` без включённых предупреждений промотировать
# нечего.
function(like_nes_vendored_target)
    foreach(t IN LISTS ARGN)
        like_nes_strip_warning_level(${t})
        set_property(TARGET ${t} APPEND PROPERTY COMPILE_OPTIONS ${LIKE_NES_NO_WARN})
    endforeach()
endfunction()

# Вендоренный исходник рядом с нашим: single-header реализации stb и miniaudio, imgui. Собирается
# ОТДЕЛЬНОЙ OBJECT-целью, а не свойством исходника, потому что уровень предупреждений — свойство
# цели, и снять его с одного файла нечем: `set_source_files_properties(... /w)` кладёт глушилку
# ПОВЕРХ каталожного `/W4`, а MSVC отвечает на это `D9025 overriding '/W4' with '/w'` — warning
# драйвера, мимо `/WX`, но прямо в лог, который читает сборочный гейт. Объекты OBJECT-цели
# попадают в потребителя через target_link_libraries, наши файлы в той же цели остаются строгими.
function(like_nes_vendored_object name)
    add_library(${name} OBJECT ${ARGN})
    # Свойства потребителя OBJECT-цели не достаются: пока файлы лежали внутри статической
    # библиотеки, PIC приезжал бы вместе с ней. Объект без -fPIC не линкуется в разделяемый
    # объект вовсе, и всплыло бы это на первом же MODULE-потребителе, а не здесь.
    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    like_nes_vendored_target(${name})
endfunction()

like_nes_silence_dependencies("${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_options(${LIKE_NES_WARN_FLAGS})
if(LIKE_NES_WERROR)
    add_compile_options(${LIKE_NES_WARN_ERROR})
endif()
