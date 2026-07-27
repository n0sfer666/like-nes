# Строгий режим компилятора — аналог тайпчекера с включённым strict: предупреждение = ошибка
# сборки. До этого дерево собиралось на уровне по умолчанию, где -Wall выключен, и варнинг
# «'%s' может обрезать 255 байт» месяцами жил в логе Ubuntu-CI, видимый только глазами.
#
# Файл подключается ПОСЛЕ FetchContent и ДО add_subdirectory нашего кода — в этом вся механика:
# add_compile_options действует на текущий каталог и на подкаталоги, добавленные ПОСЛЕ вызова,
# поэтому строгость достаётся ровно нашему коду, а уже добавленным зависимостям — нет.

option(LIKE_NES_WERROR "Compiler warnings in our own code are build errors" ON)

if(MSVC)
    set(LIKE_NES_WARN_FLAGS /W4)
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
function(like_nes_silence_dependencies dir)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t IN LISTS targets)
        get_target_property(type ${t} TYPE)
        # У INTERFACE-целей и custom-таргетов нет своей компиляции — set_property на них
        # либо бессмыслен, либо ошибка конфигурации.
        if(NOT type STREQUAL "INTERFACE_LIBRARY" AND NOT type STREQUAL "UTILITY")
            get_target_property(opts ${t} COMPILE_OPTIONS)
            if(opts)
                list(FILTER opts EXCLUDE REGEX "${LIKE_NES_WARN_LEVEL_RE}")
                set_property(TARGET ${t} PROPERTY COMPILE_OPTIONS ${opts})
            endif()
            get_target_property(flags ${t} COMPILE_FLAGS)
            if(flags)
                string(REGEX REPLACE "${LIKE_NES_WARN_LEVEL_RE}" " " flags "${flags}")
                set_property(TARGET ${t} PROPERTY COMPILE_FLAGS "${flags}")
            endif()
            set_property(TARGET ${t} APPEND PROPERTY COMPILE_OPTIONS ${LIKE_NES_NO_WARN})
        endif()
    endforeach()
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(s IN LISTS subdirs)
        like_nes_silence_dependencies("${s}")
    endforeach()
endfunction()

# Вендоренный исходник, который собирает НАША цель: single-header реализации stb и miniaudio,
# транскодер basisu, imgui. Каталог зависимостей их не покрывает — глушатся точечно на месте
# объявления цели (свойства исходника действуют только в своём каталоге).
function(like_nes_vendored_sources)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${LIKE_NES_NO_WARN}")
endfunction()

function(like_nes_vendored_target)
    foreach(t IN LISTS ARGN)
        set_property(TARGET ${t} APPEND PROPERTY COMPILE_OPTIONS ${LIKE_NES_NO_WARN})
    endforeach()
endfunction()

like_nes_silence_dependencies("${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_options(${LIKE_NES_WARN_FLAGS})
if(LIKE_NES_WERROR)
    add_compile_options(${LIKE_NES_WARN_ERROR})
endif()
