add_library(ach_core STATIC
  registry.cpp tracker.cpp state.cpp store.cpp
  manifest.cpp bake.cpp bake_parse.cpp
  delivery.cpp)
target_include_directories(ach_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# PUBLIC: bundle_fixture.hpp (заголовок фикстуры) тоже открывает файлы через шов.
target_link_libraries(ach_core PUBLIC asset_hash platform_core)

add_executable(achievements_test achievements_test.cpp)
target_link_libraries(achievements_test PRIVATE ach_core)

add_executable(ach_state_test state_test.cpp)
target_link_libraries(ach_state_test PRIVATE ach_core)

add_executable(ach_crash_test crash_test.cpp)
target_link_libraries(ach_crash_test PRIVATE ach_core platform_core)

add_executable(ach_manifest_test manifest_test.cpp)
target_link_libraries(ach_manifest_test PRIVATE ach_core)

# Шов бейка (спека #10, гейт 4): таблица достижений живёт в бандле #5 и читается
# из mmap-региона без парсинга. Вслед за asset_core — на всех трёх ОС (спека #12, гейт 1).
add_library(ach_bundle STATIC bundle_source.cpp)
target_link_libraries(ach_bundle PUBLIC ach_core asset_core)

add_executable(ach_bundle_test bundle_test.cpp)
target_link_libraries(ach_bundle_test PRIVATE ach_bundle)

add_executable(ach_delivery_test delivery_test.cpp)
target_link_libraries(ach_delivery_test PRIVATE ach_core)

add_executable(ach_delivery_lifecycle_test delivery_lifecycle_test.cpp)
target_link_libraries(ach_delivery_lifecycle_test PRIVATE ach_core)

# Плагинный шов (спека #10, гейт 5): бэкенд приезжает плагином на точке расширения #6.
add_library(ach_plugin STATIC plugin_backend.cpp)
# plugin_core — PUBLIC: бэкенд поднимает потребитель, и `PluginHost` он держит сам.
target_link_libraries(ach_plugin PUBLIC ach_core plugin_core)

# Загрузка бэкенда — через platform_module внутри PluginHost, сам тест модуль не открывает
# (одно открытие = один экземпляр, см. platform_module.hpp). Шов приезжает с ach_plugin.
add_executable(ach_steam_test steam_test.cpp)
target_link_libraries(ach_steam_test PRIVATE ach_plugin)
