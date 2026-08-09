# --- framework_physics: детерминированная 2D-физика (спека #15), вертикаль 2 — выпуклые формы,
# вращение, постоянный манифольд. Зависимость только вниз, на framework_core: движковые подсистемы о
# слое не знают, и инвариант 1 спеки #14 проверяет это грепом по этой самой строке линковки.
add_library(framework_physics STATIC
  axis_terms.cpp body.cpp broadphase.cpp cache.cpp gap.cpp mass.cpp narrowphase.cpp nearest.cpp
  prepare.cpp sat.cpp shape.cpp solver.cpp state_hash.cpp world.cpp)
target_include_directories(framework_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_physics PUBLIC framework_core)

add_executable(framework_physics_test framework_physics_test.cpp)
target_link_libraries(framework_physics_test PRIVATE framework_physics platform_core)

# Гейт 2 спеки #15: перетасованный порядок создания обязан дать тот же хеш. Отдельной целью, а не
# случаем в общем тесте, — чтобы имя упавшей цели в логе CI уже называло сломанный инвариант.
add_executable(framework_physics_order_test framework_physics_order_test.cpp)
target_link_libraries(framework_physics_order_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_rt_test framework_physics_rt_test.cpp)
target_link_libraries(framework_physics_rt_test PRIVATE framework_physics platform_core)

# Геометрия узкой фазы отдельно от сцены: голден отвечает «три ОС сошлись», эти цели — «сошлись на
# верном». Манифольд из одной точки там, где обязано быть две, обе платформы посчитают одинаково.
#
# Целей ТРИ, по числу путей узкой фазы, и у каждого свой прод-файл: точечное ядро (`nearest.cpp`),
# отсечение граней (`sat.cpp`), зазор ядер (`gap.cpp`). Пути разные по существу — расстояние против
# проекций, — и имя упавшей цели в логе CI обязано называть сломанный, а не «узкую фазу вообще».
add_executable(framework_physics_point_test framework_physics_point_test.cpp)
target_link_libraries(framework_physics_point_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_sat_test framework_physics_sat_test.cpp)
target_link_libraries(framework_physics_sat_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_gap_test framework_physics_gap_test.cpp)
target_link_libraries(framework_physics_gap_test PRIVATE framework_physics platform_core)

# Границы заявленного диапазона ПРОГОНАМИ: масса у обоих клампов, скорость у потолка. Отдельной
# целью и отдельной сценой: golden гейта 1 закрепляет ОДНУ сцену из середины диапазона, и именно
# поэтому он молча пропустил насыщение импульса под тяжёлым телом.
add_executable(framework_physics_range_test framework_physics_range_test.cpp)
target_link_libraries(framework_physics_range_test PRIVATE framework_physics platform_core)

# Коэффициенты оси — тоже без единого шага мира, и это отдельная цель, а не случай в гейте выше.
# Оба дефекта вертикали 2 жили в этой арифметике, а наблюдались через сцену, где «доля равна нулю» и
# «доля вдвое меньше нужной» выглядят одинаково. Имя цели обязано называть слой, на котором сломано.
add_executable(framework_physics_terms_test framework_physics_terms_test.cpp)
target_link_libraries(framework_physics_terms_test PRIVATE framework_physics platform_core)

# Клампы конструктора — без единого шага мира. Имя упавшей цели в логе CI обязано отличать
# «решатель не доехал на границе» от «вход в диапазон вообще не привели».
add_executable(framework_physics_clamp_test framework_physics_clamp_test.cpp)
target_link_libraries(framework_physics_clamp_test PRIVATE framework_physics platform_core)
