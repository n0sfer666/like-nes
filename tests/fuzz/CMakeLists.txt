# Гейт 9 (спека #21): устойчивость десериализаторов к битому вводу.
#
# ОДИН бинарь на все цели, а не бинарь на читателя: свой мутатор, свой список seed'ов и свой
# порог на случай иначе размножились бы по десятку целей и разъехались молча. Список целей —
# в коде (`fuzz_target.hpp`), а не в CMake: шаг CI со своим перечнем расходится с деревом первым.
add_executable(fuzz_readers
  fuzz_readers.cpp
  fuzz_targets.cpp
  fuzz_targets_ach.cpp
  fuzz_targets_engine.cpp
  fuzz_targets_framework.cpp
  fuzz_targets_plugin.cpp)

# Включения — С ПРЕФИКСОМ КАТАЛОГА от `engine/`, потому что в дереве по два `bake.hpp`,
# `manifest.hpp`, `registry.hpp` и `state.hpp` (у материалов и достижений, у достижений и
# плагинов, у достижений и персонажа). Каталог достижений НЕ добавляется в пути намеренно — это
# требование его собственного CMakeLists: их внутренние включения разрешаются относительно
# включающего файла и попадают куда надо сами, а общий -I отдавал бы «ближайший» заголовок.
target_include_directories(fuzz_readers PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/engine)

# Порядок — ОТ ЗАВИСЯЩЕГО К ЗАВИСИМОСТИ, и это не вкусовщина. CMake сохраняет заявленный порядок
# прямых зависимостей, а чтобы статический архив стоял после того, кто из него тянет символы,
# дописывает его на линию ВТОРОЙ раз. `material_core` перед `material_bake` (а `framework_tilemap`
# перед `framework_character`) давал ровно это, и ld64 отвечал `ignoring duplicate libraries` —
# предупреждение, которое сборочный гейт не умеет считать неважным, и правильно делает.
# Список собирается переменной, а не вторым вызовом `target_link_libraries`, по той же причине:
# сцена IDE тянет материалы, тайлмап и платформу своим замыканием и обязана быть в НАЧАЛЕ списка.
# `Threads::Threads` — из-за сторожевого потока (`fuzz_watchdog.hpp`): порог на случай проверяется
# только после возврата читателя, а не вернувшийся читатель прерывается вторым потоком.
# `find_package(Threads REQUIRED)` уже сделан в корневом CMakeLists.
set(FUZZ_LIBS
  asset_core
  ach_core
  framework_character framework_graphics framework_input framework_tilemap
  material_bake material_core
  plugin_core
  platform_core
  Threads::Threads)

# Сцена IDE приезжает вместе с `IDE_POC`: без него `scene_core` не собирается вовсе, а задача
# Windows CI с clang-cl и ASan гоняется с `-DIDE_POC=OFF` осознанно. Число целей печатает сам
# прогон, поэтому в логе видно, какая конфигурация проверена, а не только что она зелёная.
if(IDE_POC)
  target_sources(fuzz_readers PRIVATE fuzz_targets_scene.cpp)
  target_compile_definitions(fuzz_readers PRIVATE LN_FUZZ_SCENE=1)
  list(PREPEND FUZZ_LIBS scene_core)
endif()

target_link_libraries(fuzz_readers PRIVATE ${FUZZ_LIBS})
