# --- Источники света (спека #18, вертикаль 3) ---
# Свет — ДАННЫЕ: таблица в бандле, читаемая zero-parse, а не список, зашитый в рендерер. До этого
# шага источников было ровно три, и лежали они литералом в `engine/render/scene.cpp`.
#
# Две цели по той же границе, что у материалов: рантайм читает таблицу из mmap и текстового
# парсера не содержит вовсе — иначе разбор исходников уезжал бы в каждую игру, которая всего лишь
# рисует.
add_library(light_core STATIC table.cpp)
target_include_directories(light_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Лексический слой берётся у соседней подсистемы отдельной целью (`material_text`), а не пишется
# здесь третьей копией `trim`/`split`/`parse_float`. Ровно эта копия и была дефектом, который
# свернул cf94c9d: расхождение между копиями не ловилось ничем, и фикс дошёл до трёх из четырёх.
# Ребро одностороннее и оплачено тем, что свет — часть той же спеки #18, что и материалы.
add_library(light_bake STATIC bake.cpp bake_parse.cpp)
target_link_libraries(light_bake PUBLIC light_core material_text platform_core)

add_executable(light_table_test light_table_test.cpp)
target_link_libraries(light_table_test PRIVATE light_bake)
