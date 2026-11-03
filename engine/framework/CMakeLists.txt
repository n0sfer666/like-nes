# --- Слой gameplay-фреймворка (спека #14). Правила слоя — README.md рядом.
# Модуль на подкаталог: раунды #15–#18 добавляют свои, не трогая соседей.
add_subdirectory(core)
add_subdirectory(save)
add_subdirectory(input)
add_subdirectory(touch)
add_subdirectory(haptic)
add_subdirectory(physics)
add_subdirectory(depth)
add_subdirectory(character)
add_subdirectory(tilemap)
add_subdirectory(graphics)
add_subdirectory(ui)
add_subdirectory(music)
add_subdirectory(hits)
add_subdirectory(rollback)
add_subdirectory(replay)
add_subdirectory(scene)
add_subdirectory(kinematic)

# Выравнивание базы у ЧЕТЫРЁХ читателей секций (аудит #21, A·2·6) — одной целью над модулями:
# дефект один и тот же строкой в каждом читателе, и цель на модуль утверждала бы его четырежды.
add_executable(framework_section_align_test framework_section_align_test.cpp)
target_link_libraries(framework_section_align_test PRIVATE framework_input framework_graphics
  kit_platformer framework_character framework_tilemap platform_core)
