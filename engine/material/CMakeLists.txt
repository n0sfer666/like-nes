# --- Материалы (спека #18): материал = шейдер + параметры как ДАННЫЕ ---
# Две цели, а не одна: рантайм читает таблицу из mmap-региона и текстового парсера не содержит
# вовсе (решение 3 спеки — компиляция и разбор офлайн, в `assetc`). Слитые в одну цель, они тянули
# бы разбор исходников в каждую игру, которая всего лишь рисует.
add_library(material_core STATIC param.cpp table.cpp)
target_include_directories(material_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Файлы пекарь читает ЧЕРЕЗ платформенный шов (`platform_core`), а не `fopen`: MSVC на нём
# ругается депрекацией, а `/W4 /WX` раннера превращает её в упавшую сборку.
add_library(material_bake STATIC bake.cpp bake_parse.cpp text.cpp)
target_link_libraries(material_bake PUBLIC material_core platform_core)

add_executable(material_bake_test material_bake_test.cpp)
target_link_libraries(material_bake_test PRIVATE material_bake)

add_executable(material_source_test material_source_test.cpp)
target_link_libraries(material_source_test PRIVATE material_bake)

add_executable(material_table_test material_table_test.cpp)
target_link_libraries(material_table_test PRIVATE material_bake)

# Мирроринг «слот параметра ↔ смещение в шейдере». Отдельной целью от трёх выше по тому же
# контракту, что развёл гейты профиля в #16: имя упавшей цели обязано называть класс поломки, а
# здесь он свой — не пекарь и не читатель разъехались, а библиотека разъехалась со своим WGSL.
# Цель читает файлы библиотеки, поэтому путь к каталогу передаётся аргументом (по умолчанию —
# относительный от корня дерева, как у бандл-гейтов).
add_executable(material_library_test material_library_test.cpp)
target_link_libraries(material_library_test PRIVATE material_bake platform_core)

# Шов «материал → пайплайн» (гейты 4/5/6 спеки #18). Третья цель, а не часть `material_core`:
# ядро читает таблицу и не знает про GPU вовсе — им пользуются пекарь и тесты, которым WebGPU не
# нужен, а на мобильных шеллах его ещё и нет. Текстового парсера здесь тоже нет: кэш получает
# готовую таблицу и текст модуля, а откуда они взялись — не его вопрос.
add_library(material_gpu STATIC cache.cpp pipeline.cpp wgpu_error.cpp)
target_link_libraries(material_gpu PUBLIC material_core webgpu)

# Диагностика шейдера — своей целью БЕЗ webgpu. Не мелочность: её линкует тест совместимости с
# панелью редактора (#7), у которого ни устройства, ни адаптера нет и быть не должно, — а тест,
# ради формата строки поднимающий GPU, не запускается там, где формат как раз и разъезжается.
add_library(material_diag STATIC diag.cpp)
target_include_directories(material_diag PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Кросс-бэкендная валидация библиотеки (гейт 2 спеки #18). Отдельно от `material_gpu`, потому что
# потребитель у неё один и он не рисует: пекарь. Кэшу она не нужна, а тянуть в каждую игру
# перебор бэкендов значило бы поднимать в рантайме три инстанса ради вопроса, отвеченного в бейке.
add_library(material_validate STATIC validate.cpp validate_device.cpp)
target_link_libraries(material_validate PUBLIC material_gpu material_diag)
