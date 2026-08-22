# --- framework_character: кинематический контроллер персонажа (спека #16, вертикаль 1). Зависит
# ВНИЗ, на framework_physics: контроллер спрашивает мир шейпкастами #15 и не знает ни о слое, ни об
# игре — инвариант 1 спеки #14 проверяет направление грепом по этой самой строке линковки.
add_library(framework_character STATIC controller.cpp profile.cpp slide.cpp trajectory.cpp)
target_include_directories(framework_character PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_character PUBLIC framework_physics)

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
