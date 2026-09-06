# Гейты сети игры-образца (спека #22): каждая цель поднимает ПАРУ ПРОЦЕССОВ над сценой платформера
# и говорит с соседом настоящим сокетом. Своим файлом, а не строками в CMakeLists.txt рядом: тот
# упёрся в жёсткий предел длины ровно на второй из них, а граница между ними и соседями всё равно
# есть — это единственные цели каталога, которым нужны net_core и запуск ребёнка.
#
# Двоичный файл у каждой один на обе роли: `--peer send|recv` перезапускает СОБСТВЕННЫЙ exe, потому
# что fork'а на Windows нет (`platform_process.hpp`).
#
# Целей три, и предмет у каждой свой:
#   game_platformer_net_test        — гейты 1 и 7: из одного ввода два адресных пространства
#                                     приходят в одно состояние, и глухота соседа лечится сама;
#   game_platformer_net_direct_test — гейт 9, адресация: сосед может быть НАЗВАН аргументами, без
#                                     общей файловой системы, которой у двух машин нет по
#                                     определению;
#   game_platformer_net_hooks_test  — гейт 9, шов: прогон ЧЕРЕЗ `PeerHooks` (ввод живой половины и
#                                     показ кадра) — тот же прогон, что и без него.
# Объявлены ОДНИМ `foreach`, потому что у них общий список исходников и общий список библиотек:
# разбор `--peer` живёт заголовком `platformer_peer_argv.hpp`, и вторая копия правил сборки
# разъехалась бы с первой молча — ровно так же, как копия разбора молча игнорировала бы чужие флаги.
foreach(gate net_test net_direct_test net_hooks_test)
  add_executable(game_platformer_${gate}
    platformer_${gate}.cpp platformer_peer.cpp platformer_sim.cpp platformer_scene.cpp
    platformer_level.cpp)
  target_include_directories(game_platformer_${gate} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(game_platformer_${gate} PRIVATE framework_character framework_tilemap
    framework_rollback framework_replay framework_alloc_probe_control net_core asset_core
    platform_core Threads::Threads)
endforeach()
