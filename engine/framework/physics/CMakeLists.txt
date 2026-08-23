# --- framework_physics: детерминированная 2D-физика (спека #15), вертикаль 2 — выпуклые формы,
# вращение, постоянный манифольд. Зависимость только вниз, на framework_core: движковые подсистемы о
# слое не знают, и инвариант 1 спеки #14 проверяет это грепом по этой самой строке линковки.
add_library(framework_physics STATIC
  axis_terms.cpp body.cpp broadphase.cpp cache.cpp cast.cpp distance.cpp event_hash.cpp events.cpp
  gap.cpp island.cpp mass.cpp narrowphase.cpp nearest.cpp prepare.cpp query.cpp query_index.cpp
  rest.cpp sat.cpp shape.cpp solver.cpp state_hash.cpp world.cpp)
target_include_directories(framework_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(framework_physics PUBLIC framework_core)

add_executable(framework_physics_test framework_physics_test.cpp)
target_link_libraries(framework_physics_test PRIVATE framework_physics platform_core)

# Гейт 2 спеки #15: перетасованный порядок создания обязан дать тот же хеш. Отдельной целью, а не
# случаем в общем тесте, — чтобы имя упавшей цели в логе CI уже называло сломанный инвариант.
add_executable(framework_physics_order_test framework_physics_order_test.cpp)
target_link_libraries(framework_physics_order_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_rt_test framework_physics_rt_test.cpp)
target_link_libraries(framework_physics_rt_test PRIVATE framework_physics platform_core
  framework_alloc_probe_control)

# Геометрия узкой фазы отдельно от сцены: голден отвечает «три ОС сошлись», эти цели — «сошлись на
# верном». Манифольд из одной точки там, где обязано быть две, обе платформы посчитают одинаково.
#
# Целей ТРИ, по числу путей узкой фазы, и у каждого свой прод-файл: точечное ядро (`nearest.cpp`),
# отсечение граней (`sat.cpp`), зазор ядер (`gap.cpp`). Пути разные по существу — расстояние против
# проекций, — и имя упавшей цели в логе CI обязано называть сломанный, а не «узкую фазу вообще».
add_executable(framework_physics_point_test framework_physics_point_test.cpp)
target_link_libraries(framework_physics_point_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_sat_test framework_physics_sat_test.cpp)
target_link_libraries(framework_physics_sat_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_gap_test framework_physics_gap_test.cpp)
target_link_libraries(framework_physics_gap_test PRIVATE framework_physics platform_core)

# Границы заявленного диапазона ПРОГОНАМИ: масса у обоих клампов, скорость у потолка. Отдельной
# целью и отдельной сценой: golden гейта 1 закрепляет ОДНУ сцену из середины диапазона, и именно
# поэтому он молча пропустил насыщение импульса под тяжёлым телом.
add_executable(framework_physics_range_test framework_physics_range_test.cpp)
target_link_libraries(framework_physics_range_test PRIVATE framework_physics platform_core)

# Коэффициенты оси — тоже без единого шага мира, и это отдельная цель, а не случай в гейте выше.
# Оба дефекта вертикали 2 жили в этой арифметике, а наблюдались через сцену, где «доля равна нулю» и
# «доля вдвое меньше нужной» выглядят одинаково. Имя цели обязано называть слой, на котором сломано.
add_executable(framework_physics_terms_test framework_physics_terms_test.cpp)
target_link_libraries(framework_physics_terms_test PRIVATE framework_physics platform_core)

# Клампы конструктора — без единого шага мира. Имя упавшей цели в логе CI обязано отличать
# «решатель не доехал на границе» от «вход в диапазон вообще не привели».
add_executable(framework_physics_clamp_test framework_physics_clamp_test.cpp)
target_link_libraries(framework_physics_clamp_test PRIVATE framework_physics platform_core)

# Гейт 4 спеки #15: запросы к миру против таблицы эталонных ответов, включая касательные и
# вырожденные раскладки. Отдельно от узкой фазы, хотя зовёт её же: у запроса своя арифметика —
# продвижение свипа, — и своё соглашение о нормали, обратное манифольдному.
add_executable(framework_physics_query_test framework_physics_query_test.cpp)
target_link_libraries(framework_physics_query_test PRIVATE framework_physics platform_core)

# Перекрытие области — третий вид запроса и своя цель. Вопрос у него не «когда коснёмся», а «кто
# здесь сейчас»: ответ не одиночный, а СПИСОК, и у списка есть собственный контракт — порядок по
# ключу и очистка приёмника. В общей цели со свипами упавший порядок назывался бы «запросы сломаны».
add_executable(framework_physics_overlap_test framework_physics_overlap_test.cpp)
target_link_libraries(framework_physics_overlap_test PRIVATE framework_physics platform_core)

# Индекс запросов: полоса вместо обхода мира. Своей целью, потому что спрашивает она не «верен ли
# ответ» — на это отвечают три цели выше, — а «во сколько он обошёлся и не устарел ли». Оба вопроса
# краснеют молча: подорожавший запрос остаётся верным, а протухший индекс даёт не ошибку, а тело,
# которого запрос не видит там, куда его только что переставили.
add_executable(framework_physics_index_test framework_physics_index_test.cpp)
target_link_libraries(framework_physics_index_test PRIVATE framework_physics platform_core)

# Слои, маски и триггеры. Отдельная цель, потому что это правило ОТБРАСЫВАНИЯ: сломанный фильтр не
# даёт неверное число, он даёт отсутствие контакта, а отсутствие в общем тесте выглядит как «сцена
# просто такая». Проверяется парой «отфильтровано / не отфильтровано» на одной и той же геометрии.
add_executable(framework_physics_filter_test framework_physics_filter_test.cpp)
target_link_libraries(framework_physics_filter_test PRIVATE framework_physics platform_core)

# События контакта: порядок фаз во времени и порядок внутри кадра. Голден потока живёт здесь же и
# только здесь — в отличие от голдена состояния, который сверяют между ОС по печатному выводу,
# событийный сверяется собственным утверждением, а оно одинаково на всех трёх.
add_executable(framework_physics_event_test framework_physics_event_test.cpp)
target_link_libraries(framework_physics_event_test PRIVATE framework_physics platform_core)

# Гейт 3 спеки #15: башня стоит. Отдельная цель, потому что вопрос у неё не про значение, а про
# ОТСУТСТВИЕ дрейфа за длинный прогон: сломанный тёплый старт даёт не другое число, а стопку, которая
# расползается тем заметнее, чем дольше смотришь. В общей цели такое читалось бы как «сцена такая».
#
# Целей ДВЕ, по числу входов тёплого старта: у поставленной впритык башни импульсы растут с нуля в
# тишине, у собранной падением — приходят ударами. Сцена и мерки общие
# (`framework_physics_stack_scene.hpp`), потому что вторая цель сверяет свою раскладку покоя с первой.
add_executable(framework_physics_stack_test framework_physics_stack_test.cpp)
target_link_libraries(framework_physics_stack_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_impact_test framework_physics_impact_test.cpp)
target_link_libraries(framework_physics_impact_test PRIVATE framework_physics platform_core)

# Гейт 5 спеки #15: сон не влияет на результат. Отдельная цель, потому что сверяются ДВА прогона
# одной сцены, и упасть она может по причине, которой нет ни у одной другой: экономия, изменившая
# результат, — это не «неверное число», а расхождение двух миров, которые обязаны совпадать.
add_executable(framework_physics_sleep_test framework_physics_sleep_test.cpp)
target_link_libraries(framework_physics_sleep_test PRIVATE framework_physics platform_core)

# Спекулятивная полоса: одно число, продетое сквозь ВСЕ три пути узкой фазы и через два разных входа
# (шаг с полем, запросы с нулём). Отдельная цель, потому что сломанное сравнение с полем даёт не
# неверную геометрию, а пропавшую опору — в сцене это неотличимо от «ящик просто падает».
add_executable(framework_physics_band_test framework_physics_band_test.cpp)
target_link_libraries(framework_physics_band_test PRIVATE framework_physics platform_core)

# Три двери пробуждения замершего острова: тяготение, сдвинутая статика, неконстантная ручка тела.
# Отдельная цель, потому что её отказ выглядит не отказом: незакрытая дверь не даёт ни исключения, ни
# расхождения хеша — она даёт ящик, висящий в воздухе там, где из-под него убрали пол. Голдену сцены
# такого не увидеть, он сверяет прогон, в котором обстановку никто не менял.
#
# Третья дверь — своей целью: там утверждение двустороннее (чтение обязано стоить ноль, запись —
# доехать), обе стороны были дефектами, и обе мерятся числами, которых у первых двух дверей нет.
add_executable(framework_physics_wake_test framework_physics_wake_test.cpp)
target_link_libraries(framework_physics_wake_test PRIVATE framework_physics platform_core)

add_executable(framework_physics_handle_test framework_physics_handle_test.cpp)
target_link_libraries(framework_physics_handle_test PRIVATE framework_physics platform_core)

# Глубина стека, которую держит решатель. Отдельная цель, потому что гейт стека прибит к шести
# ящикам и про границу не спрашивает: башня из девяти заваливалась при штатном трении молча, а
# `VELOCITY_ITERATIONS` меняли, ни разу не спросив, сколько ящиков после этого стоит.
add_executable(framework_physics_depth_test framework_physics_depth_test.cpp)
target_link_libraries(framework_physics_depth_test PRIVATE framework_physics platform_core)

# Гейт 8 спеки #15: цена шага на целевом числе тел. Отдельная цель, потому что мерит она не то же,
# что все остальные: те спрашивают «верно ли», эта — «сколько стоит». Утверждает счётчики работы,
# время печатает — обоснование этого разделения в самом файле и в `counters.hpp`.
add_executable(framework_physics_perf_test framework_physics_perf_test.cpp)
target_link_libraries(framework_physics_perf_test PRIVATE framework_physics platform_core
  framework_alloc_probe_control)
