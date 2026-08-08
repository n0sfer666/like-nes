# --- framework_physics: детерминированная 2D-физика (спека #15), вертикаль 1 — круг, AABB,
# импульсный решатель. Зависимость только вниз, на framework_core: движковые подсистемы о слое не
# знают, и инвариант 1 спеки #14 проверяет это грепом по этой самой строке линковки.
add_library(framework_physics STATIC
  body.cpp broadphase.cpp narrowphase.cpp solver.cpp state_hash.cpp world.cpp)
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

# Границы заявленного диапазона (масса у обоих клампов, скорость у потолка, клампы материала и
# формы). Отдельной целью и отдельной сценой: golden гейта 1 закрепляет ОДНУ сцену из середины
# диапазона, и именно поэтому он молча пропустил насыщение импульса под тяжёлым телом.
add_executable(framework_physics_range_test framework_physics_range_test.cpp)
target_link_libraries(framework_physics_range_test PRIVATE framework_physics platform_core)
