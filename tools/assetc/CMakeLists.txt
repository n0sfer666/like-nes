# Пекари без GPU и без таблиц игры (спека #24, В4): своя цель, чтобы тесты манифеста и пути pixel
# не линковали webgpu, который нужен исполняемому `assetc` только для валидации материалов. Жанра
# в ядре нет (спека #29, В2а): игровые виды записей и секции `--game` приносит инструмент игры
# таблицей, а ядро едет в desktop-префикс SDK как `like-nes::assetc_core`.
add_library(assetc_core STATIC bakers.cpp bakers_pixel.cpp assetc_manifest.cpp
  assetc_manifest_paths.cpp assetc_depfile.cpp assetc_emit.cpp assetc_tool.cpp manifest_bake.cpp
  manifest_kind_bake.cpp manifest_mode.cpp game_mode.cpp verify_game.cpp level_source.cpp bakers_level.cpp
  bakers_clips.cpp bakers_fonts.cpp bakers_credits.cpp bakers_touch.cpp push_table.cpp)
target_include_directories(assetc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(assetc_core PUBLIC asset_core framework_core framework_tiled framework_graphics_import
  framework_touch)

# Пекари zero-parse таблиц — только в дереве: их зовут базовый `assetc` (материалы и светы) и
# `ugly_assetc` (секции игры-образца), а SDK их не ставит.
add_library(assetc_tables STATIC bakers_table.cpp)
target_link_libraries(assetc_tables PUBLIC assetc_core ach_core material_bake light_bake framework_input)

# Гейт 1 спеки #12: пекарь собирается и печёт на всех трёх ОС — запуск кодеков ушёл
# за platform_process, открытие файлов за platform_fs.
add_executable(assetc assetc_main.cpp validate_materials.cpp)
# Гейт 2 спеки #18: пекарь ВАЛИДИРУЕТ библиотеку эффектов, а значит линкует webgpu. Цена честная:
# без устройства ни один бэкенд не скажет, собирается ли шейдер, а внешнего валидатора WGSL в
# дереве нет — `tint` не установлен ни на одном раннере и не был бы гейтом, он был бы декорацией.
# Бейк от этого не перестаёт работать на машине без GPU: бэкенды, которых нет, честно называются
# пропущенными, и число проверенных печатается строкой.
target_link_libraries(assetc PRIVATE assetc_tables material_validate)
target_copy_webgpu_binaries(assetc)
reproducible_rpath(assetc)

add_executable(clip_dump clip_dump.cpp)
target_link_libraries(clip_dump PRIVATE assetc_core)

add_executable(assetc_manifest_test assetc_manifest_test.cpp)
target_link_libraries(assetc_manifest_test PRIVATE assetc_core)
add_executable(assetc_kinds_test assetc_kinds_test.cpp)
target_link_libraries(assetc_kinds_test PRIVATE assetc_core)
add_executable(assetc_bake_test assetc_bake_test.cpp)
target_link_libraries(assetc_bake_test PRIVATE assetc_core)
add_executable(assetc_level_test assetc_level_test.cpp)
target_link_libraries(assetc_level_test PRIVATE assetc_core)
add_executable(assetc_clips_test assetc_clips_test.cpp)
target_link_libraries(assetc_clips_test PRIVATE assetc_core)
add_executable(assetc_font_test assetc_font_test.cpp)
target_link_libraries(assetc_font_test PRIVATE assetc_core)
add_executable(assetc_touch_test assetc_touch_test.cpp)
target_link_libraries(assetc_touch_test PRIVATE assetc_core)
