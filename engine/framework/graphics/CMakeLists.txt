# --- framework_graphics: анимации, камера, нарезка атласа и отрисовочные примитивы (спека #17).
# РЕНДЕРА среди зависимостей нет, и это не упущение: шкала клипа считается целыми числами тиков, а
# регион атласа здесь — прямоугольник в пикселях, а не текстура. Тянуть сюда RHI «на будущее»
# значило бы связать анимацию с устройством вывода ровно тем ребром, которое инвариант 1 спеки #14
# запрещает в обратную сторону.
add_library(framework_graphics STATIC clip.cpp player.cpp machine.cpp camera.cpp atlas_parse.cpp atlas_bake.cpp atlas_read.cpp debug_draw.cpp sprite.cpp nine_slice.cpp
  particles.cpp viewport.cpp)
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

# Отладочная отрисовка (шаг E) — ДВЕ цели по тому же основанию, что у нарезки выше: геометрия и
# голден потока квадов отдельно от ОТКАЗОВ (переполнение буфера, вырожденный отрезок, атлас без
# шрифта). Обе headless: построитель не знает ни про RHI, ни про батч.
add_executable(framework_graphics_debug_test framework_graphics_debug_test.cpp)
target_link_libraries(framework_graphics_debug_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_debug_refusal_test framework_graphics_debug_refusal_test.cpp)
target_link_libraries(framework_graphics_debug_refusal_test PRIVATE framework_graphics platform_core)

# Спрайты, слои и батчи (вертикаль 2, шаг A) — ТРИ цели, граница та же: класс поломки.
#   * ..._batch_test         — ПОРЯДОК, число draw-call'ов (гейт 6) и раскладка 9-slice + голден;
#   * ..._batch_refusal_test — ОТКАЗЫ: переполнение списка, переполнение батчей, буферов нет;
#   * ..._batch_alloc_test   — ГЕЙТ 8: установившийся кадр не ходит в кучу.
# Третья отделена от первых двух вынужденно, а не по вкусу: `framework_alloc_probe.hpp` подменяет
# глобальные operator new/delete всей программе и допустим ровно в одной цели.
add_executable(framework_graphics_batch_test framework_graphics_batch_test.cpp)
target_link_libraries(framework_graphics_batch_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_batch_refusal_test framework_graphics_batch_refusal_test.cpp)
target_link_libraries(framework_graphics_batch_refusal_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_batch_alloc_test framework_graphics_batch_alloc_test.cpp)
target_link_libraries(framework_graphics_batch_alloc_test PRIVATE framework_graphics platform_core
  framework_alloc_probe_control)

# Шов «тайлы → спрайты» (вертикаль 2, шаг B) — ОТДЕЛЬНАЯ цель, а не файл в `framework_graphics`.
# Сетка тянет за собой физику, и затащи её в саму библиотеку — клипы, машина и камера линковали бы
# тайлы с решателем, не спросив ни одного тайла. Ровно тот же довод, по которому RHI нет среди
# зависимостей выше.
add_library(framework_graphics_tiles STATIC tile_draw.cpp)
target_include_directories(framework_graphics_tiles PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_graphics_tiles PUBLIC framework_graphics framework_tilemap)

# Две цели по классу отказа: culling и голден отдельно от ОТКАЗОВ (вид мимо карты, переполнение
# списка, тайл с битами вне таблицы).
# `platform_core` берётся ЗАГОЛОВКАМИ, а не третьей строкой линковки, и это вынужденно: сюда он и
# так приезжает дважды — `framework_graphics` и `framework_tilemap` держат его PRIVATE каждый, — а
# третье упоминание в списке заставляет линкер macOS сказать `ld: warning: ignoring duplicate
# libraries` и уронить сборочный гейт предупреждением, к нашему коду отношения не имеющим. Тело
# `platform::Args` приезжает по любому из двух рёбер, а PRIVATE-ребро не несёт каталога включений —
# его и дописываем.
add_executable(framework_graphics_tile_test framework_graphics_tile_test.cpp)
target_link_libraries(framework_graphics_tile_test PRIVATE framework_graphics_tiles)
target_include_directories(framework_graphics_tile_test PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)

add_executable(framework_graphics_tile_refusal_test framework_graphics_tile_refusal_test.cpp)
target_link_libraries(framework_graphics_tile_refusal_test PRIVATE framework_graphics_tiles)
target_include_directories(framework_graphics_tile_refusal_test PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)

# Частицы обоих классов (вертикаль 2, шаг C) — ТРИ цели, граница та же: класс поломки.
#   * ..._particle_test         — ДЕТЕРМИНИЗМ геймплейного класса (гейт 4), геометрия шага, отрисовка;
#   * ..._particle_split_test   — ГЕЙТ 3: декорация не доезжает до sim-хеша, и разделение — в типе;
#   * ..._particle_refusal_test — ОТКАЗЫ: нет пула, нет таблицы, описание вне её, потолок подачи.
# Сцена у первых двух общая (`framework_graphics_particle_scene.hpp`) вынужденно: гейт 3 сравнивает
# ДВА прогона, и сцена, написанная в каждой цели заново, сравнивала бы разные сцены молча.
add_executable(framework_graphics_particle_test framework_graphics_particle_test.cpp)
target_link_libraries(framework_graphics_particle_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_particle_split_test framework_graphics_particle_split_test.cpp)
target_link_libraries(framework_graphics_particle_split_test PRIVATE framework_graphics platform_core)

add_executable(framework_graphics_particle_refusal_test framework_graphics_particle_refusal_test.cpp)
target_link_libraries(framework_graphics_particle_refusal_test PRIVATE framework_graphics
  platform_core)

# Зум (вертикаль 2, шаг D) — ТРИ цели, граница та же, что у частиц: класс поломки.
#   * ..._viewport_test         — ПЕРЕВОД координат и пиксельная сетка, плюс голден развёртки;
#   * ..._zoom_reach_test       — до кого зум ДОЕХАЛ: окно culling'а и границы уровня;
#   * ..._viewport_refusal_test — ОТКАЗЫ: нулевой и отрицательный зум, ppu, свёрнутое окно.
# Второй линкует `framework_graphics_tiles`, а первый — нет, и это не мелочь: голден перевода
# координат не имеет права падать оттого, что разъехалась сетка.
add_executable(framework_graphics_viewport_test framework_graphics_viewport_test.cpp)
target_link_libraries(framework_graphics_viewport_test PRIVATE framework_graphics framework_physics
  platform_core)

add_executable(framework_graphics_zoom_reach_test framework_graphics_zoom_reach_test.cpp)
target_link_libraries(framework_graphics_zoom_reach_test PRIVATE framework_graphics_tiles)
target_include_directories(framework_graphics_zoom_reach_test PRIVATE
  $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)

add_executable(framework_graphics_viewport_refusal_test
  framework_graphics_viewport_refusal_test.cpp)
target_link_libraries(framework_graphics_viewport_refusal_test PRIVATE framework_graphics
  platform_core)
