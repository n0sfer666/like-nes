# --- Материалы (спека #18): материал = шейдер + параметры как ДАННЫЕ ---
# Две цели, а не одна: рантайм читает таблицу из mmap-региона и текстового парсера не содержит
# вовсе (решение 3 спеки — компиляция и разбор офлайн, в `assetc`). Слитые в одну цель, они тянули
# бы разбор исходников в каждую игру, которая всего лишь рисует.
add_library(material_core STATIC param.cpp table.cpp)
target_include_directories(material_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(material_bake STATIC bake.cpp bake_parse.cpp text.cpp)
target_link_libraries(material_bake PUBLIC material_core)

add_executable(material_bake_test material_bake_test.cpp)
target_link_libraries(material_bake_test PRIVATE material_bake)

add_executable(material_source_test material_source_test.cpp)
target_link_libraries(material_source_test PRIVATE material_bake)

add_executable(material_table_test material_table_test.cpp)
target_link_libraries(material_table_test PRIVATE material_bake)

