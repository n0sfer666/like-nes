# Гейт 1 спеки #12: пекарь собирается и печёт на всех трёх ОС — запуск кодеков ушёл
# за platform_process, открытие файлов за platform_fs.
add_executable(assetc assetc_main.cpp bakers.cpp bakers_table.cpp validate_materials.cpp
  verify_game.cpp)
# Гейт 2 спеки #18: пекарь ВАЛИДИРУЕТ библиотеку эффектов, а значит линкует webgpu. Цена честная:
# без устройства ни один бэкенд не скажет, собирается ли шейдер, а внешнего валидатора WGSL в
# дереве нет — `tint` не установлен ни на одном раннере и не был бы гейтом, он был бы декорацией.
# Бейк от этого не перестаёт работать на машине без GPU: бэкенды, которых нет, честно называются
# пропущенными, и число проверенных печатается строкой.
target_link_libraries(assetc PRIVATE asset_core ach_core material_bake material_validate
  framework_input framework_character framework_tilemap framework_graphics)
target_copy_webgpu_binaries(assetc)
reproducible_rpath(assetc)
