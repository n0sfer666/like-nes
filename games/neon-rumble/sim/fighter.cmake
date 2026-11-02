if(TARGET rumble_fighter)
  return()
endif()
set(_fighter_dir "${CMAKE_CURRENT_LIST_DIR}")
add_library(rumble_fighter STATIC
  ${_fighter_dir}/fighter_parse.cpp ${_fighter_dir}/fighter_move_parse.cpp
  ${_fighter_dir}/fighter_chain.cpp ${_fighter_dir}/fighter_bake.cpp ${_fighter_dir}/fighter_read.cpp)
target_include_directories(rumble_fighter PUBLIC "${_fighter_dir}")
set_target_properties(rumble_fighter PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_features(rumble_fighter PUBLIC cxx_std_20)
if(TARGET like-nes::engine)
  target_include_directories(rumble_fighter PUBLIC
    $<TARGET_PROPERTY:like-nes::engine,INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_options(rumble_fighter PRIVATE $<TARGET_PROPERTY:like-nes::engine,INTERFACE_COMPILE_OPTIONS>)
  target_link_libraries(rumble_fighter PUBLIC like-nes::framework_graphics like-nes::framework_depth like-nes::framework_core)
else()
  target_link_libraries(rumble_fighter PUBLIC framework_graphics framework_depth framework_core)
endif()
unset(_fighter_dir)
