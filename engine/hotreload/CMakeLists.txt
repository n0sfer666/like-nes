# Hot-reload PoC. Загрузка модуля и крэш-изоляция ушли за швы platform_core (спека #12):
# dlopen/siglongjmp на POSIX, LoadLibraryExW+копия/SEH на Windows → гейт на всех 3 ОС.
# game_crash оптимизируется как все: сбой записан через volatile — см. engine/plugin.
add_library(game_a MODULE game_a.cpp)
add_library(game_b MODULE game_b.cpp)
add_library(game_crash MODULE game_crash.cpp)
foreach(M game_a game_b game_crash)
  # Нужен только platform_export.h (макрос экспорта), объекты шва модулю ни к чему.
  target_include_directories(${M} PRIVATE ${CMAKE_SOURCE_DIR}/engine/platform)
  set_target_properties(${M} PROPERTIES PREFIX "")
endforeach()

add_executable(hotreload_host host.cpp)
target_link_libraries(hotreload_host PRIVATE platform_core)
