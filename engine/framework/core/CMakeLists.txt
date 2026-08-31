# --- framework_core: стадии тика и расписание систем (спека #14, правила слоя — ../README.md).
# Зависит только от engine_core: стрелка слоя направлена вниз, и проверить это должно быть можно
# по этой строке, не читая код.
add_library(framework_core STATIC schedule.cpp fixmath.cpp fixtrig.cpp text_fields.cpp)
target_include_directories(framework_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_core PUBLIC engine_core)

# --- Позитивный контроль счётчика аллокаций. Отдельной целью, а не файлом внутри framework_core:
# код тестовый, и в прод-библиотеке ему делать нечего. Отдельной от теста — потому что барьер
# нужен именно межмодульный: тело, видимое вызывающему, оба компилятора выбрасывают вместе с
# выделением (разбор и замеры — в шапке framework_alloc_probe_control.hpp).
#
# Цель НЕ линкует framework_core и не должна: `framework_alloc_probe.hpp` ОПРЕДЕЛЯЕТ глобальные
# operator new/delete и живёт ровно в одном TU на программу — здешние выделения видны счётчику
# через замещение на линковке, тем же путём, каким их видит проверяемый код.
add_library(framework_alloc_probe_control STATIC framework_alloc_probe_control.cpp)
target_include_directories(framework_alloc_probe_control PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(framework_schedule_test framework_schedule_test.cpp)
target_link_libraries(framework_schedule_test PRIVATE framework_core platform_core)

# Свойства таблицы синуса — отдельной целью, а не случаем внутри теста физики: имя упавшей цели в
# логе CI обязано называть сломанный слой, а поворот держит и форму, и решатель.
add_executable(framework_trig_test framework_trig_test.cpp)
target_link_libraries(framework_trig_test PRIVATE framework_core platform_core)

# Текстовый слой пекарей — своей целью, а не случаем внутри чужого теста: спрашивают его четыре
# модуля, и падать он обязан именем слоя, а не именем первого пекаря, чей голден разошёлся.
add_executable(framework_text_test framework_text_test.cpp)
target_link_libraries(framework_text_test PRIVATE framework_core platform_core)
