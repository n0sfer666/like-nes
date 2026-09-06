# Примеры документации (гейт 2 спеки #19): каждый файл здесь обязан собираться и ЗАПУСКАТЬСЯ, а его
# вывод — совпадать с голденом `<имя>.out`, который тот же документ и показывает читателю.
#
# Список целей ВЫВОДИТСЯ обходом каталога, а не пишется руками: рукописный отстал бы от первого же
# нового примера, и гейт молча перестал бы его собирать — ровно `list-drift` из `ci_lint.py`.
# `CONFIGURE_DEPENDS` нужен потому, что иначе добавленный файл не появился бы до ручной
# переконфигурации, то есть локальный прогон и CI расходились бы на свежем примере.
#
# Каталог добавляется ПОСЛЕ `cmake/warnings.cmake`, как и весь наш код: пример, показанный читателю,
# обязан проходить `-Wall -Wextra -Werror` наравне с движком.
file(GLOB LIKE_NES_DOC_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(src ${LIKE_NES_DOC_EXAMPLES})
    get_filename_component(name "${src}" NAME_WE)
    add_executable(doc_example_${name} "${src}")
    target_link_libraries(doc_example_${name} PRIVATE framework_core platform_core)
endforeach()
