# Пекари без GPU и без таблиц игры (спека #24, В4): своя цель, чтобы тесты манифеста и пути pixel
# не линковали webgpu, который нужен исполняемому `assetc` только для валидации материалов.
add_library(assetc_bake STATIC bakers.cpp bakers_pixel.cpp assetc_manifest.cpp
  assetc_manifest_paths.cpp assetc_depfile.cpp assetc_emit.cpp manifest_bake.cpp)
target_include_directories(assetc_bake PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(assetc_bake PUBLIC asset_core framework_core)

# Гейт 1 спеки #12: пекарь собирается и печёт на всех трёх ОС — запуск кодеков ушёл
# за platform_process, открытие файлов за platform_fs.
add_executable(assetc assetc_main.cpp bakers_table.cpp manifest_mode.cpp validate_materials.cpp
  verify_game.cpp)
# Гейт 2 спеки #18: пекарь ВАЛИДИРУЕТ библиотеку эффектов, а значит линкует webgpu. Цена честная:
# без устройства ни один бэкенд не скажет, собирается ли шейдер, а внешнего валидатора WGSL в
# дереве нет — `tint` не установлен ни на одном раннере и не был бы гейтом, он был бы декорацией.
# Бейк от этого не перестаёт работать на машине без GPU: бэкенды, которых нет, честно называются
# пропущенными, и число проверенных печатается строкой.
target_link_libraries(assetc PRIVATE assetc_bake ach_core material_bake material_validate
  light_bake framework_input framework_character framework_tilemap framework_graphics)
target_copy_webgpu_binaries(assetc)
reproducible_rpath(assetc)

add_executable(assetc_manifest_test assetc_manifest_test.cpp)
target_link_libraries(assetc_manifest_test PRIVATE assetc_bake)
add_executable(assetc_bake_test assetc_bake_test.cpp)
target_link_libraries(assetc_bake_test PRIVATE assetc_bake)
