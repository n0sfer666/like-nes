# --- framework_graphics: анимации, камера и отрисовочные примитивы (спека #17). Зависимостей у
# библиотеки НЕТ, и это не упущение: шкала клипа считается целыми числами тиков, а регион атласа
# здесь — номер, а не текстура. Ни физики, ни платформы, ни тем более RHI модулю не нужно, и
# добавлять их «на будущее» значило бы связать анимацию с устройством вывода ровно тем ребром,
# которое инвариант 1 спеки #14 запрещает в обратную сторону.
add_library(framework_graphics STATIC clip.cpp player.cpp machine.cpp)
target_include_directories(framework_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Гейты шага A — ТРИ цели, и граница между ними класс поломки, который обязано называть имя
# упавшей цели в логе CI:
#   * ..._clip_test  — ШКАЛА: кадр как функция от тика, пинг-понг, вырожденные клипы;
#   * ..._event_test — ДОСТАВКА меток: ровно на том тике и ровно столько раз, плюс усечение;
#   * ..._anim_test  — ГОЛДЕН состояния (гейт 1 спеки): три ОС сошлись на одном числе.
# Первые две от третьей отделены тем же доводом, что голден траектории #16 от гейта окон: голден
# отвечает «машины сошлись», а не «сошлись на верном» — шкала, сдвинутая на тик, даёт стабильный
# хеш на всех трёх.
add_executable(framework_graphics_clip_test framework_graphics_clip_test.cpp)
target_link_libraries(framework_graphics_clip_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_event_test framework_graphics_event_test.cpp)
target_link_libraries(framework_graphics_event_test PRIVATE framework_graphics platform_core)

# framework_physics — только здесь и только за `hash_mix.hpp`: свёртка обязана быть общей с
# физическим голденом (два голдена на разных константах несравнимы), а тянуть её в саму библиотеку
# значило бы связать анимацию с телами. Ребро направлено вниз, инвариант 1 спеки #14 цел.
add_executable(framework_graphics_anim_test framework_graphics_anim_test.cpp)
target_link_libraries(framework_graphics_anim_test PRIVATE framework_graphics framework_physics
  platform_core)

# Машина разделена на два гейта по КЛАССУ ОТКАЗА: «выбрала не тот переход» видно по номеру
# состояния, «переключилась не в тот тик» — только по номеру тика, и в одном файле второе тонет.
add_executable(framework_graphics_pick_test framework_graphics_pick_test.cpp)
target_link_libraries(framework_graphics_pick_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_hold_test framework_graphics_hold_test.cpp)
target_link_libraries(framework_graphics_hold_test PRIVATE framework_graphics platform_core)
