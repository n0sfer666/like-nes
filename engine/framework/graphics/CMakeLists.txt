# --- framework_graphics: анимации, камера, нарезка атласа и отрисовочные примитивы (спека #17).
# РЕНДЕРА среди зависимостей нет, и это не упущение: шкала клипа считается целыми числами тиков, а
# регион атласа здесь — прямоугольник в пикселях, а не текстура. Тянуть сюда RHI «на будущее»
# значило бы связать анимацию с устройством вывода ровно тем ребром, которое инвариант 1 спеки #14
# запрещает в обратную сторону.
add_library(framework_graphics STATIC clip.cpp player.cpp machine.cpp camera.cpp atlas_text.cpp
  atlas_parse.cpp atlas_bake.cpp atlas_read.cpp)
target_include_directories(framework_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# framework_core — за `fix32` и `Vec2`: камера живёт в мировых координатах, а вторая реализация
# детерминированной векторной арифметики была бы вторым КОНТРАКТОМ, расходящимся на округлении.
# platform_core — PRIVATE и только за файловым швом пекаря нарезки: наружу из модуля торчат клип,
# камера и таблица регионов, а не чтение файла. То же устройство, что у framework_tilemap.
target_link_libraries(framework_graphics PUBLIC framework_core PRIVATE platform_core)

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

# Камера разделена на четыре цели тем же доводом, что и клипы, — по КЛАССУ ОТКАЗА:
#   * ..._camera_test — ПОЛИТИКИ и их порядок: «камера в нужном месте», видно по координате;
#   * ..._shake_test  — ТРЯСКА: «дрожит как заказано и только в отрисовке», по координате не видно;
#   * ..._pixel_test  — СЕТКА И ПАРАЛЛАКС: «не дрожит относительно соседа», видно только на ходу;
#   * ..._path_test   — ГОЛДЕН траектории: «три машины сошлись», за верность не отвечает вовсе.
add_executable(framework_graphics_camera_test framework_graphics_camera_test.cpp)
target_link_libraries(framework_graphics_camera_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_pixel_test framework_graphics_pixel_test.cpp)
target_link_libraries(framework_graphics_pixel_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_shake_test framework_graphics_shake_test.cpp)
target_link_libraries(framework_graphics_shake_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_path_test framework_graphics_path_test.cpp)
target_link_libraries(framework_graphics_path_test PRIVATE framework_graphics framework_physics
  platform_core)

# Нарезка атласа как испечённые ДАННЫЕ (шаг D) — ТРИ цели, и граница между ними та же, что у карт
# #16: имя упавшей цели в логе CI обязано называть класс поломки.
#   * ..._atlas_test         — round-trip и голден байтов: доехало ли написанное и цела ли раскладка;
#   * ..._atlas_refusal_test — ОТКАЗЫ: номер строки у пекаря, порченая секция у читателя;
#   * ..._atlas_bundle_test  — секция из ЛЕЖАЩЕГО В GIT game.bundle против разбора atlas.txt: доехало
#     ли испечённое до читателя и перепечён ли бандл после правки исходника.
add_executable(framework_graphics_atlas_test framework_graphics_atlas_test.cpp)
target_link_libraries(framework_graphics_atlas_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_atlas_refusal_test framework_graphics_atlas_refusal_test.cpp)
target_link_libraries(framework_graphics_atlas_refusal_test PRIVATE framework_graphics
  platform_core)

# asset_core — только здесь: бандл открывается менеджером ассетов, и тянуть его в саму библиотеку
# значило бы связать нарезку с форматом хранения. Направление зависимостей цело — инвариант 1 спеки
# #14 запрещает обратное ребро, от подсистем к слою.
add_executable(framework_graphics_atlas_bundle_test framework_graphics_atlas_bundle_test.cpp)
target_link_libraries(framework_graphics_atlas_bundle_test PRIVATE framework_graphics asset_core
  platform_core)
