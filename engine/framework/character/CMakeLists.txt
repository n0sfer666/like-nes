# --- framework_character: кинематический контроллер персонажа (спека #16, вертикаль 1). Зависит
# ВНИЗ, на framework_physics: контроллер спрашивает мир шейпкастами #15 и не знает ни о слое, ни об
# игре — инвариант 1 спеки #14 проверяет направление грепом по этой самой строке линковки.
add_library(framework_character STATIC controller.cpp profile.cpp slide.cpp trajectory.cpp
  profile_text.cpp profile_parse.cpp profile_bake.cpp profile_read.cpp)
target_include_directories(framework_character PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# platform_core — PRIVATE: наружу из модуля торчит профиль, а не файловый шов, которым пекарь
# читает манифест. То же устройство, что у framework_input.
target_link_libraries(framework_character PUBLIC framework_physics PRIVATE platform_core)

# Гейт 1 спеки #16: записанный ввод даёт ту же траекторию побитово на трёх ОС.
add_executable(framework_character_test framework_character_test.cpp)
target_link_libraries(framework_character_test PRIVATE framework_character platform_core)

# Гейт 2: окна прощения срабатывают РОВНО в заявленном окне тиков — на границе и за ней. Отдельной
# целью от голдена, потому что голден отвечает «три ОС сошлись», а не «сошлись на верном»: окно,
# укоротившееся на тик, даёт стабильный хеш на всех трёх машинах.
add_executable(framework_character_window_test framework_character_window_test.cpp)
target_link_libraries(framework_character_window_test PRIVATE framework_character platform_core)

# Прыжок ЦЕЛЕВОЙ ВЫСОТОЙ: вершина совпадает с `jump_height` в пределах, выведенных в profile.cpp,
# и удержание кнопки меняет высоту. Требование спеки записано в юнитах, поэтому и проверяется
# юнитами, а не хешем.
add_executable(framework_character_jump_test framework_character_jump_test.cpp)
target_link_libraries(framework_character_jump_test PRIVATE framework_character platform_core)

# Инвариант 2 спеки #16: на максимальной скорости сквозь тонкую стену прохода нет.
add_executable(framework_character_tunnel_test framework_character_tunnel_test.cpp)
target_link_libraries(framework_character_tunnel_test PRIVATE framework_character platform_core)

# Инвариант 5 спеки #16: тик не ходит в кучу. Счётчик и его позитивный контроль общие с гейтом 6
# спеки #15 — `framework_alloc_probe.hpp` подменяет глобальные operator new/delete и допустим ровно
# в одном TU на программу, поэтому цель отдельная, а не случай в голдене.
add_executable(framework_character_alloc_test framework_character_alloc_test.cpp)
target_link_libraries(framework_character_alloc_test PRIVATE framework_character platform_core
  framework_alloc_probe_control)

# Профиль как испечённые ДАННЫЕ (решение 2 спеки #16) — ТРИ цели, и граница между ними та же, что у
# пресетов ввода #14: имя упавшей цели в логе CI обязано называть класс поломки.
#   * ..._profile_test — round-trip, голден байтов, отказы читателя (согласованы ли пекарь с
#     читателем и не поехала ли раскладка);
#   * ..._profile_refusal_test — отказы пекаря НОМЕРОМ СТРОКИ (не испёкся ли манифест молча);
#   * ..._bundle_test — секция из ЛЕЖАЩЕГО В GIT game.bundle против default_profile() (доехало ли
#     испечённое до рантайма, и перепечён ли бандл после правки манифеста).
add_executable(framework_character_profile_test framework_character_profile_test.cpp)
target_link_libraries(framework_character_profile_test PRIVATE framework_character platform_core)

add_executable(framework_character_profile_refusal_test
               framework_character_profile_refusal_test.cpp)
target_link_libraries(framework_character_profile_refusal_test PRIVATE framework_character
  platform_core)

# asset_core — только здесь: бандл открывается менеджером ассетов, и тянуть его в саму библиотеку
# значило бы связать профиль с форматом хранения. Направление зависимостей это не ломает —
# инвариант 1 спеки #14 запрещает обратное ребро, от подсистем к слою.
add_executable(framework_character_bundle_test framework_character_bundle_test.cpp)
target_link_libraries(framework_character_bundle_test PRIVATE framework_character asset_core
  platform_core)
