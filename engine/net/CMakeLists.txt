# Надёжный слой поверх UDP (спека #22). Зависит ВНИЗ — только на платформенный шов; ни framework,
# ни симуляция сюда не входят, потому что по сети идёт ввод, а не мир (решение 1 спеки).
add_library(net_core STATIC net_link.cpp)
target_include_directories(net_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(net_core PUBLIC platform_net)
set_target_properties(net_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Проводной формат и окно подтверждений: арифметика без транспорта и без пира.
add_executable(net_wire_test net_wire_test.cpp)
target_link_libraries(net_wire_test PRIVATE net_core)

# Гейт 3 спеки: потери, дубли и перестановки детерминированным инъектором. Транспорта нет
# намеренно — красный прогон обязан означать дефект, а не невезение живой сети.
add_executable(net_link_test net_link_test.cpp)
target_link_libraries(net_link_test PRIVATE net_core)

# Он же поверх настоящего сокета: границу Writer -> sendto -> recvfrom -> Reader инъектор не
# воспроизводит, потому что её в нём нет.
add_executable(net_loopback_test net_loopback_test.cpp)
target_link_libraries(net_loopback_test PRIVATE net_core Threads::Threads)
