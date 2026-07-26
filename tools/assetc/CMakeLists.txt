if(NOT WIN32)
  add_executable(assetc assetc_main.cpp bakers.cpp)
  target_link_libraries(assetc PRIVATE asset_core ach_core)
endif()
