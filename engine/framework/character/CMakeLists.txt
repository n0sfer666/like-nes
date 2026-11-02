# Общее геометрическое ядро; контроллер и профиль принадлежат games/kits/platformer.
add_library(framework_character STATIC collision.cpp slide.cpp support.cpp push.cpp)
target_include_directories(framework_character PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_character PUBLIC framework_physics framework_tilemap)

add_executable(framework_character_core_test framework_character_core_test.cpp)
target_link_libraries(framework_character_core_test PRIVATE framework_character)
