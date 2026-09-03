# Гейты сети игры-образца (спека #22): две цели, каждая поднимает ПАРУ ПРОЦЕССОВ над сценой
# платформера и говорит с соседом настоящим сокетом. Своим файлом, а не строками в CMakeLists.txt
# рядом: тот упёрся в жёсткий предел длины ровно на второй из них, а граница между ними и соседями
# всё равно есть — это единственные цели каталога, которым нужны net_core и запуск ребёнка.
# Гейты 1 и 7 спеки #22: два ПРОЦЕССА над той же сценой, канал между ними — настоящий сокет.
# Отдельной целью от гейта отката выше, потому что предмет другой: тот утверждает, что снимок сцены
# ничего не теряет, этот — что из одного ввода два адресных пространства приходят в одно состояние.
# Двоичный файл один на обе роли: `--peer send|recv` перезапускает СОБСТВЕННЫЙ exe, потому что
# fork'а на Windows нет (`platform_process.hpp`).
add_executable(game_platformer_net_test
  platformer_net_test.cpp platformer_peer.cpp platformer_sim.cpp platformer_scene.cpp
  platformer_level.cpp)
target_include_directories(game_platformer_net_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_net_test PRIVATE framework_character framework_tilemap
  framework_rollback framework_replay framework_alloc_probe_control net_core asset_core platform_core
  Threads::Threads)

# Третья роль того же приёма: гейт 9 спеки #22 — пиры находят друг друга по НАЗВАННЫМ адресам, без
# общего файла. Отдельной целью от гейта выше, потому что утверждение другое (сосед может быть
# назван, а не «два процесса сошлись»), а разбор `--peer` у обеих общий и живёт заголовком
# platformer_peer_argv.hpp — копия в каждой цели молча игнорировала бы чужие флаги.
add_executable(game_platformer_net_direct_test
  platformer_net_direct_test.cpp platformer_peer.cpp platformer_sim.cpp platformer_scene.cpp
  platformer_level.cpp)
target_include_directories(game_platformer_net_direct_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_platformer_net_direct_test PRIVATE framework_character framework_tilemap
  framework_rollback framework_replay framework_alloc_probe_control net_core asset_core platform_core
  Threads::Threads)
