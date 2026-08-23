# --- framework_tilemap: тайловая сетка коллизии (спека #16, вертикаль 2, шаг A). Зависит ВНИЗ, на
# framework_physics: сетка не пишет геометрию заново, а разворачивает тайл в форму #15 и отвечает
# теми же свипом и узкой фазой. Обратного ребра нет — физика про тайлы не знает (решение 3 спеки).
add_library(framework_tilemap STATIC grid.cpp query.cpp)
target_include_directories(framework_tilemap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_tilemap PUBLIC framework_physics)

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
