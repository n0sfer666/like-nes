# Каталога достижений нет в путях потребителей: у ach_core и plugin_core совпадают имена
# registry.hpp/manifest.hpp, и общий -I отдавал бы «ближайший», а не нужный. Все включения
# извне — с префиксом каталога (`../achievements/...`), внутри каталога работают относительные.
add_library(ach_core STATIC
  registry.cpp tracker.cpp state.cpp store.cpp
  manifest.cpp bake.cpp bake_parse.cpp
  delivery.cpp)

add_executable(achievements_test achievements_test.cpp)
target_link_libraries(achievements_test PRIVATE ach_core)

add_executable(ach_state_test state_test.cpp)
target_link_libraries(ach_state_test PRIVATE ach_core)

add_executable(ach_crash_test crash_test.cpp)
target_link_libraries(ach_crash_test PRIVATE ach_core)

add_executable(ach_manifest_test manifest_test.cpp)
target_link_libraries(ach_manifest_test PRIVATE ach_core)

# Шов бейка (спека #10, гейт 4): таблица достижений живёт в бандле #5 и читается
# из mmap-региона без парсинга. Зависит от asset_core → только POSIX (как и он).
if(NOT WIN32)
  add_library(ach_bundle STATIC bundle_source.cpp)
  target_link_libraries(ach_bundle PUBLIC ach_core asset_core)

  add_executable(ach_bundle_test bundle_test.cpp)
  target_link_libraries(ach_bundle_test PRIVATE ach_bundle)
endif()

add_executable(ach_delivery_test delivery_test.cpp)
target_link_libraries(ach_delivery_test PRIVATE ach_core)

add_executable(ach_delivery_lifecycle_test delivery_lifecycle_test.cpp)
target_link_libraries(ach_delivery_lifecycle_test PRIVATE ach_core)

# Плагинный шов (спека #10, гейт 5): бэкенд приезжает плагином на точке расширения #6.
add_library(ach_plugin STATIC plugin_backend.cpp)
# plugin_core — PRIVATE: линк протянется как $<LINK_ONLY:>, а его include-путь наружу не пойдёт.
# Иначе engine/plugin/sim.hpp затеняет sim.hpp игры у потребителей — та же коллизия, что убрана у ach_core.
target_link_libraries(ach_plugin PUBLIC ach_core PRIVATE plugin_core)

if(NOT WIN32)
  add_executable(ach_steam_test steam_test.cpp)
  target_link_libraries(ach_steam_test PRIVATE ach_plugin ${CMAKE_DL_LIBS})
endif()
