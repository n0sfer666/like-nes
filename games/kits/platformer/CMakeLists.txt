# Контроллер и профиль платформера: один код в дереве и против установленного SDK.
add_library(kit_platformer STATIC controller.cpp profile.cpp assist.cpp ladder.cpp trajectory.cpp
  profile_parse.cpp profile_bake.cpp profile_read.cpp)
target_include_directories(kit_platformer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(kit_platformer PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(TARGET like-nes::engine)
  target_link_libraries(kit_platformer PUBLIC like-nes::engine)
else()
  target_link_libraries(kit_platformer PUBLIC framework_character PRIVATE platform_core)
endif()

# Тесты кита запускает корень дерева; потребителю SDK нужны только исходники библиотеки.
if(TARGET framework_character)
  add_subdirectory(tests)
endif()
