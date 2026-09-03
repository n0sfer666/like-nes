# --- framework_rollback: кольцо снимков, кольцо ввода и план переигрывания (вертикаль 1 спеки #22).
# Зависимостей нет ВОВСЕ, и это утверждение, а не недосмотр: откат не знает ни про физику, ни про
# fix32, ни про сеть — он знает только «симуляцию с save/restore/step». Появившееся здесь ребро к
# симуляции означало бы, что откатный прогон пошёл не тем кодом, что одиночный.
add_library(framework_rollback STATIC plan.cpp)
target_include_directories(framework_rollback PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Целей ДВЕ, по числу вопросов. План отвечает «арифметика верна» и падает с именем сломанного
# правила; сессия отвечает «прогон с задержкой сошёлся» и падает расхождением хеша, под которым
# лежит любая из полудюжины причин. В одной цели имя первой терялось бы в формулировке второй.
add_executable(framework_rollback_plan_test framework_rollback_plan_test.cpp)
target_link_libraries(framework_rollback_plan_test PRIVATE framework_rollback)

add_executable(framework_rollback_test framework_rollback_test.cpp)
target_link_libraries(framework_rollback_test PRIVATE framework_rollback framework_core
  platform_core framework_alloc_probe_control)
