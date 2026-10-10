# --- framework_ai: FSM мозгов, жетоны атаки и ГСЧ мира (спека #25, В5). Всё — симуляция: в снапшоте,
# в хеше и без float, как драка, поверх которой стоит.
add_library(framework_ai STATIC world_rng.cpp attack_tokens.cpp ai_brain.cpp ai_state.cpp ai_step.cpp ai_target.cpp)
target_include_directories(framework_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_ai PUBLIC framework_brawl)

add_executable(framework_ai_test framework_ai_test.cpp framework_ai_target_test.cpp)
target_link_libraries(framework_ai_test PRIVATE framework_ai)

add_executable(framework_ai_resim_test framework_ai_resim_test.cpp)
target_link_libraries(framework_ai_resim_test PRIVATE framework_ai framework_rollback)
