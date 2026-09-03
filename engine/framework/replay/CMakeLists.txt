# --- framework_replay: поток реплея и его верификация (вертикаль 2 спеки #22).
# Зависимостей нет ВОВСЕ, по тому же основанию, что у `framework_rollback`: верификация знает
# только «симуляцию со `step` и `hash`», и появившееся здесь ребро к сцене или к сети означало бы,
# что переигранный тик пошёл не тем кодом, что живой.
add_library(framework_replay INTERFACE)
target_include_directories(framework_replay INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Игрушечная симуляция берётся у гейта отката: второй экземпляр той же фикстуры был бы второй
# правдой о том, что такое «тот же прогон».
add_executable(framework_replay_test framework_replay_test.cpp)
target_link_libraries(framework_replay_test PRIVATE framework_replay framework_rollback)
