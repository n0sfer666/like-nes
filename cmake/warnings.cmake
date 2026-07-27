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
