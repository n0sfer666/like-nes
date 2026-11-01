add_library(framework_hits STATIC hit_box.cpp struck_list.cpp hitstop.cpp)
target_include_directories(framework_hits PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_hits PUBLIC framework_depth framework_graphics)

foreach(part box struck hitstop)
  add_executable(framework_hits_${part}_test framework_hits_${part}_test.cpp)
  target_link_libraries(framework_hits_${part}_test PRIVATE framework_hits)
  target_include_directories(framework_hits_${part}_test PRIVATE
    $<TARGET_PROPERTY:platform_core,INTERFACE_INCLUDE_DIRECTORIES>)
endforeach()
