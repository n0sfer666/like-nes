# Пекарь игры-образца (спека #29, В2а): секции `game.bundle` — таблица игры поверх ядра пекаря.
# Режимы `--game` и `--verify-game` ушли сюда из базового `assetc`, movement — вместе с ними.
add_executable(ugly_assetc ugly_assetc.cpp movement_section.cpp)
target_link_libraries(ugly_assetc PRIVATE assetc_tables framework_character)
