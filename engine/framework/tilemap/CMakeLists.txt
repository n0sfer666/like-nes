# --- framework_tilemap: тайловая сетка коллизии (спека #16, вертикаль 2, шаг A). Зависит ВНИЗ, на
# framework_physics: сетка не пишет геометрию заново, а разворачивает тайл в форму #15 и отвечает
# теми же свипом и узкой фазой. Обратного ребра нет — физика про тайлы не знает (решение 3 спеки).
add_library(framework_tilemap STATIC grid.cpp query.cpp map_text.cpp map_parse.cpp map_bake.cpp
  map_read.cpp)
target_include_directories(framework_tilemap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# platform_core — PRIVATE: наружу из модуля торчит сетка, а не файловый шов, которым пекарь читает
# исходник. То же устройство, что у framework_character и framework_input.
target_link_libraries(framework_tilemap PUBLIC framework_physics PRIVATE platform_core)

# Гейт шага A: три запроса против ответов, посчитанных ИЗ РАСКЛАДКИ, плюс голден свёртки всех
# ответов. Контроли внутри парные: зонд в воздухе обязан отвечать пусто, фильтр без совпадений —
# тоже, а счётчик рассмотренных тайлов обязан совпасть на картах 12x12 и 200x200 и при этом быть
# меньше площади карты (инвариант 4 спеки).
add_executable(framework_tilemap_test framework_tilemap_test.cpp)
target_link_libraries(framework_tilemap_test PRIVATE framework_tilemap platform_core)

# Инварианты 2 и 3 спеки — отдельной целью, потому что проверяются они ОТСУТСТВИЕМ ответа на классе
# сцен, и у каждого утверждения о пустоте здесь своя парная сцена, где ответ обязан быть.
add_executable(framework_tilemap_seam_test framework_tilemap_seam_test.cpp)
target_link_libraries(framework_tilemap_seam_test PRIVATE framework_tilemap platform_core)

# Геометрия склона (вертикаль 3, шаг B) — своей целью: у `..._test` предмет это ОКНО и маска, а
# здесь ФОРМА, в которую разворачивается тайл, и имя упавшей цели в логе CI обязано их различать.
add_executable(framework_tilemap_slope_test framework_tilemap_slope_test.cpp)
target_link_libraries(framework_tilemap_slope_test PRIVATE framework_tilemap platform_core)

# Односторонний тайл (вертикаль 3, шаг B) — своей целью: предмет здесь не форма и не окно, а
# ПРАВИЛО ОТБОРА хита, и различать классы поломки в логе CI обязано имя цели.
add_executable(framework_tilemap_oneway_test framework_tilemap_oneway_test.cpp)
target_link_libraries(framework_tilemap_oneway_test PRIVATE framework_tilemap platform_core)

# Карта как испечённые ДАННЫЕ (вертикаль 2, шаг B) — ТРИ цели, и граница между ними та же, что у
# профиля #16: имя упавшей цели в логе CI обязано называть класс поломки.
#   * ..._bake_test — round-trip и голден байтов (согласованы ли пекарь с читателем и не поехала ли
#     раскладка);
#   * ..._refusal_test — отказы пекаря НОМЕРОМ СТРОКИ и отказы читателя на порченой секции;
#   * ..._bundle_test — секция из ЛЕЖАЩЕГО В GIT game.bundle против разбора tilemap.txt (доехало ли
#     испечённое до сетки, и перепечён ли бандл после правки исходника).
add_executable(framework_tilemap_bake_test framework_tilemap_bake_test.cpp)
target_link_libraries(framework_tilemap_bake_test PRIVATE framework_tilemap platform_core)

add_executable(framework_tilemap_refusal_test framework_tilemap_refusal_test.cpp)
target_link_libraries(framework_tilemap_refusal_test PRIVATE framework_tilemap platform_core)

# asset_core — только здесь: бандл открывается менеджером ассетов, и тянуть его в саму библиотеку
# значило бы связать карту с форматом хранения. Направление зависимостей это не ломает — инвариант 1
# спеки #14 запрещает обратное ребро, от подсистем к слою.
add_executable(framework_tilemap_bundle_test framework_tilemap_bundle_test.cpp)
target_link_libraries(framework_tilemap_bundle_test PRIVATE framework_tilemap asset_core
  platform_core)
