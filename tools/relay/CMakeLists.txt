add_library(relay_server STATIC net_relay_server.cpp net_relay_admission.cpp)
target_include_directories(relay_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(relay_server PUBLIC net_core)

add_executable(like_nes_relay relay_main.cpp)
target_link_libraries(like_nes_relay PRIVATE relay_server platform_core Threads::Threads)

add_executable(relay_server_test relay_server_test.cpp relay_limits_test.cpp)
target_link_libraries(relay_server_test PRIVATE relay_server)
