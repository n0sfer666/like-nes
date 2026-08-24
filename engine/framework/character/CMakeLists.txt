# --- framework_character: кинематический контроллер персонажа (спека #16). Зависит ВНИЗ, на
# framework_physics и framework_tilemap: контроллер спрашивает сцену свипами #15 и разворотом тайла
# в ту же форму и не знает ни о слое, ни об игре — инвариант 1 спеки #14 проверяет направление
# грепом по этой самой строке линковки. Обратного ребра нет: ни физика, ни сетка про персонажа не
# знают.
add_library(framework_character STATIC controller.cpp collision.cpp profile.cpp slide.cpp
  assist.cpp support.cpp trajectory.cpp profile_text.cpp profile_parse.cpp profile_bake.cpp
  profile_read.cpp)
target_include_directories(framework_character PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# platform_core — PRIVATE: наружу из модуля торчит профиль, а не файловый шов, которым пекарь
# читает манифест. То же устройство, что у framework_input.
target_link_libraries(framework_character PUBLIC framework_physics framework_tilemap
  PRIVATE platform_core)

# Гейт 1 спеки #16: записанный ввод даёт ту же траекторию побитово на трёх ОС.
add_executable(framework_character_test framework_character_test.cpp)
target_link_libraries(framework_character_test PRIVATE framework_character
  framework_tilemap platform_core)

# Гейт 2: окна прощения срабатывают РОВНО в заявленном окне тиков — на границе и за ней. Отдельной
# целью от голдена, потому что голден отвечает «три ОС сошлись», а не «сошлись на верном»: окно,
# укоротившееся на тик, даёт стабильный хеш на всех трёх машинах.
add_executable(framework_character_window_test framework_character_window_test.cpp)
target_link_libraries(framework_character_window_test PRIVATE framework_character
  framework_tilemap platform_core)

# Прыжок ЦЕЛЕВОЙ ВЫСОТОЙ: вершина совпадает с `jump_height` в пределах, выведенных в profile.cpp,
# и удержание кнопки меняет высоту. Требование спеки записано в юнитах, поэтому и проверяется
# юнитами, а не хешем.
add_executable(framework_character_jump_test framework_character_jump_test.cpp)
target_link_libraries(framework_character_jump_test PRIVATE framework_character
  framework_tilemap platform_core)

# Шов смешанной сцены (вертикаль 3): порядок слияния ответов тел и тайлов. Отдельной целью от
# голдена, потому что голден отвечает «траектория та же», а не «спор разобран заявленным правилом»:
# порядок наблюдаем только там, где оба источника отвечают на одной доле пути.
add_executable(framework_character_collision_test framework_character_collision_test.cpp)
target_link_libraries(framework_character_collision_test PRIVATE framework_character
  framework_tilemap platform_core)

# Геометрическое прощение (вертикаль 3): сдвиг из-под угла потолка и притяжение к полу
# срабатывают ровно в заявленном окне ЮНИТОВ. Отдельной целью от гейта окон, который меряет то же
# самое ТИКАМИ на общей сцене: здесь нужна своя геометрия — потолок с проёмом и ступенька вниз, — а
# дописывать её в общую сцену значило бы менять обстановку под голденом.
#
# Целей ДВЕ, по приёму на каждую, и граница между ними — не длина файла, а раскладка: у сдвига она
# про потолок, у притяжения про пол, и общего у сцен нет ничего, кроме клина, которым обе проверяют
# отказ по нормали. Один файл на оба приёма упирался бы в жёсткий потолок бюджета длины ровно тогда,
# когда к таблице окон добавится случай, а резать таблицу пополам ради счётчика — это и есть тот
# разрез не по смыслу, от которого предостерегает allowlist.
add_executable(framework_character_corner_test framework_character_corner_test.cpp)
target_link_libraries(framework_character_corner_test PRIVATE framework_character
  framework_tilemap platform_core)

add_executable(framework_character_snap_test framework_character_snap_test.cpp)
target_link_libraries(framework_character_snap_test PRIVATE framework_character
  framework_tilemap platform_core)

# Ходьба по склону (шаг B вертикали 3). Третьей целью, а не случаем в двух гейтах выше: вопрос у неё
# не про окно прощения, а про то, ЧЕМ грань считается, и раскладка — своя (ступенька в тайл со
# склоном на весь перепад), потому что окна прощения на ней не срабатывают вовсе.
add_executable(framework_character_slope_test framework_character_slope_test.cpp)
target_link_libraries(framework_character_slope_test PRIVATE framework_character
  framework_tilemap platform_core)

# Односторонняя платформа (шаг B вертикали 3). Четвёртой целью по тому же основанию, что и склон:
# вопрос у неё не про грань и не про окно, а про то, КОМУ тайл отвечает и как с него сойти по
# команде, и раскладка своя — платформа с просветом над сплошным полом.
add_executable(framework_character_oneway_test framework_character_oneway_test.cpp)
target_link_libraries(framework_character_oneway_test PRIVATE framework_character
  framework_tilemap platform_core)

# Движущаяся платформа (шаг B3 вертикали 3). Пятой целью по тому же основанию, что склон и
# односторонняя платформа, и с самым сильным из них: голден траектории идёт по сцене, где мир не
# шагается НИ РАЗУ, то есть про движущуюся геометрию не утверждает ничего. Раскладка своя и вынесена
# в заголовок — здесь обстановка, в гейте утверждения.
add_executable(framework_character_platform_test framework_character_platform_test.cpp)
target_link_libraries(framework_character_platform_test PRIVATE framework_character
  framework_tilemap platform_core)

# Инвариант 2 спеки #16: на максимальной скорости сквозь тонкую стену прохода нет.
add_executable(framework_character_tunnel_test framework_character_tunnel_test.cpp)
target_link_libraries(framework_character_tunnel_test PRIVATE framework_character
  framework_tilemap platform_core)

# Инвариант 5 спеки #16: тик не ходит в кучу. Счётчик и его позитивный контроль общие с гейтом 6
# спеки #15 — `framework_alloc_probe.hpp` подменяет глобальные operator new/delete и допустим ровно
# в одном TU на программу, поэтому цель отдельная, а не случай в голдене.
add_executable(framework_character_alloc_test framework_character_alloc_test.cpp)
target_link_libraries(framework_character_alloc_test PRIVATE framework_character
  framework_tilemap platform_core framework_alloc_probe_control)

# Профиль как испечённые ДАННЫЕ (решение 2 спеки #16) — ТРИ цели, и граница между ними та же, что у
# пресетов ввода #14: имя упавшей цели в логе CI обязано называть класс поломки.
#   * ..._profile_test — round-trip, голден байтов, отказы читателя (согласованы ли пекарь с
#     читателем и не поехала ли раскладка);
#   * ..._profile_refusal_test — отказы пекаря НОМЕРОМ СТРОКИ (не испёкся ли манифест молча);
#   * ..._bundle_test — секция из ЛЕЖАЩЕГО В GIT game.bundle против default_profile() (доехало ли
#     испечённое до рантайма, и перепечён ли бандл после правки манифеста).
add_executable(framework_character_profile_test framework_character_profile_test.cpp)
target_link_libraries(framework_character_profile_test PRIVATE framework_character
  framework_tilemap platform_core)

add_executable(framework_character_profile_refusal_test
               framework_character_profile_refusal_test.cpp)
target_link_libraries(framework_character_profile_refusal_test PRIVATE framework_character
  framework_tilemap platform_core)

# asset_core — только здесь: бандл открывается менеджером ассетов, и тянуть его в саму библиотеку
# значило бы связать профиль с форматом хранения. Направление зависимостей это не ломает —
# инвариант 1 спеки #14 запрещает обратное ребро, от подсистем к слою.
add_executable(framework_character_bundle_test framework_character_bundle_test.cpp)
target_link_libraries(framework_character_bundle_test PRIVATE framework_character
  framework_tilemap asset_core platform_core)
