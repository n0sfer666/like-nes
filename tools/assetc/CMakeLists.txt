# Гейт 1 спеки #12: пекарь собирается и печёт на всех трёх ОС — запуск кодеков ушёл
# за platform_process, открытие файлов за platform_fs.
add_executable(assetc assetc_main.cpp bakers.cpp bakers_table.cpp verify_game.cpp)
target_link_libraries(assetc PRIVATE asset_core ach_core framework_input framework_character
  framework_tilemap framework_graphics)
