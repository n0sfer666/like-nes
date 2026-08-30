#!/usr/bin/env bash
# Голден физики совпадает в Debug и в Release — инвариант 1 спеки #15, отдельным файлом.
#
# Отделён от `check_goldens.sh` по границе смысла, а не ради счётчика строк: остальные две проверки
# читают продукты ЧУЖИХ сборок (`build-ci`, `build-full`), а эта конфигурирует и собирает свой
# каталог под -O0 и держит собственный список целей. Список этот вдобавок связан маркером с шагом
# CI, и адрес эталона обязан указывать на файл, где список живёт, а не на файл, который его когда-то
# содержал.
#
# Зовётся напрямую (`bash scripts/check_debug_golden.sh`) и через `check_goldens.sh debug` — имя
# упавшей проверки от этого не меняется.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

# Список — только цели, лежащие на пути СОСТОЯНИЯ: дерево Debug'ом собирать незачем. Таблица синуса
# в нём потому, что вращение считается ею, и её собственное расхождение между уровнями оптимизации
# обязано быть названо ею, а не голденом сцены.
#
# `framework_physics_perf_test` в списке потому, что его эталон счётчиков — такой же голден: он
# сверяет ЧИСЛО работы, а не хеш состояния. Отдельного литерала ему не нужно — эталон внутри
# бинаря, и Debug валится тем же сравнением, что Release. Этап от него дороже секунд на тридцать:
# сцена кучи на -O0 стоит порядка 28 мс на кадр против 2.3 на -O3.
#
# Список ОБЯЗАН совпадать с `LIKE_NES_BUILD_TARGETS` Debug-шага в `.github/workflows/ci.yml`: он
# собран руками в двух местах, и разъехавшись, тихо оставляет цель без Debug-прогона на той стороне,
# где её забыли.
#
# `framework_character_profile_refusal_test` — не голден, но путь тот же: номер строки отказа даёт
# разбор числа по int64, и расхождение -O0 с -O3 на нём было бы UB, а не «другая диагностика». Плюс
# локально его до сих пор не гонял никто: только Release-цикл CI.
#
# `framework_physics_depth_test` — тоже голден, только предметом его служит ГРАНИЦА: башня
# заявленной глубины стоит, на ящик глубже валится. Граница ножевая по построению, и уровень
# оптимизации сдвинул бы её ровно так же, как сдвинул бы хеш.
# `game_platformer_sim_test` — прогон образца по уровню из бандла: те же склон, односторонняя
# площадка и перенос опорой, но СЦЕПЛЕННЫЕ в один маршрут длиной в четыреста тиков. Гейты выше
# судят по приёму за прогон, и накопленное расхождение уровней оптимизации им негде показать:
# сойдись -O0 с -O3 на каждом приёме по отдельности и разойдись на четвёртой сотне тиков — молчали
# бы все.
# Гейты вертикали 3 шага B стоят здесь по предмету, а не потому, что они «тоже про персонажа»:
# склон судится умножением fix32 с насыщением (`max_slope * |n.y|` в `slide.hpp`), односторонний
# тайл — сравнением границ, перенос опорой — сложением позиции ДО свипа. Расхождение -O0 с -O3 в
# любом из трёх было бы UB, и увидеть его больше негде: `list-drift` сверяет копии списка МЕЖДУ
# собой и про полноту не знает ничего, поэтому забытая цель молчала бы на обеих сторонах разом.
STATE_TARGETS="framework_physics_test framework_physics_point_test framework_physics_sat_test framework_physics_gap_test framework_physics_order_test framework_physics_range_test framework_physics_clamp_test framework_physics_terms_test framework_physics_rt_test framework_physics_perf_test framework_physics_query_test framework_physics_corner_test framework_tilemap_test framework_tilemap_seam_test framework_physics_overlap_test framework_physics_index_test framework_physics_filter_test framework_physics_event_test framework_physics_stack_test framework_physics_impact_test framework_physics_sleep_test framework_physics_band_test framework_physics_wake_test framework_physics_handle_test framework_physics_depth_test framework_trig_test framework_character_test framework_character_window_test framework_character_jump_test framework_character_tunnel_test framework_character_collision_test framework_character_corner_test framework_character_snap_test framework_character_profile_test framework_character_profile_refusal_test framework_tilemap_bake_test framework_tilemap_refusal_test framework_character_slope_test framework_character_oneway_test framework_character_platform_test framework_character_perf_test framework_tilemap_slope_test framework_tilemap_oneway_test framework_tilemap_ladder_test framework_character_ladder_test game_platformer_sim_test framework_graphics_clip_test framework_graphics_event_test framework_graphics_anim_test framework_graphics_path_test framework_graphics_atlas_test framework_graphics_atlas_refusal_test framework_graphics_debug_test framework_graphics_batch_test"

# Расхождение между уровнями оптимизации в целочисленной арифметике — это UB, а не «погрешность»,
# и локально оно проверяемо целиком: ни раннера, ни железа тут не нужно.
debug_golden() {
    cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null || return 1
    BUILD_DIR=build-debug LIKE_NES_BUILD_SUBSET=1 LIKE_NES_BUILD_TYPE=Debug \
        LIKE_NES_BUILD_TARGETS="$STATE_TARGETS" bash scripts/build_check.sh || return 1
    local t bin out rc=0 golden="" traj="" baked="" tiles="" maps="" plat="" anim="" cpath="" cview="" atlas="" overlay="" batch=""
    for t in $STATE_TARGETS; do
        bin=$(find build-debug -maxdepth 2 -type f -name "$t" | head -1)
        if [ -z "$bin" ]; then echo "$t не собран"; rc=1; continue; fi
        out=$("$bin") || rc=1
        printf '%s\n' "$out" | grep -q ": PASS" || { printf '%s\n' "$out"; rc=1; }
        if [ "$t" = framework_physics_test ]; then
            golden=$(printf '%s\n' "$out" | grep -o 'physics-state-hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_character_test ]; then
            traj=$(printf '%s\n' "$out" | grep -o 'character trajectory hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_character_profile_test ]; then
            baked=$(printf '%s\n' "$out" | grep -o 'hash 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_tilemap_test ]; then
            tiles=$(printf '%s\n' "$out" | grep -o 'tilemap answers hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_tilemap_bake_test ]; then
            maps=$(printf '%s\n' "$out" | grep -o 'hash 0x[0-9a-f]*')
        fi
        if [ "$t" = game_platformer_sim_test ]; then
            plat=$(printf '%s\n' "$out" | grep -o 'platformer trajectory hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_graphics_anim_test ]; then
            anim=$(printf '%s\n' "$out" | grep -o 'anim-state hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_graphics_path_test ]; then
            cpath=$(printf '%s\n' "$out" | grep -o 'camera-path hash = 0x[0-9a-f]*')
            cview=$(printf '%s\n' "$out" | grep -o 'camera-view hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_graphics_atlas_test ]; then
            atlas=$(printf '%s\n' "$out" | grep -o 'hash 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_graphics_debug_test ]; then
            overlay=$(printf '%s\n' "$out" | grep -o 'debug-overlay hash = 0x[0-9a-f]*')
        fi
        if [ "$t" = framework_graphics_batch_test ]; then
            batch=$(printf '%s\n' "$out" | grep -o 'sprite-batch hash = 0x[0-9a-f]*')
        fi
    done
    # Тот же литерал, что у Release-шага в CI. Сверять Debug сам с собой значило бы проверять, что
    # -O0 воспроизводим, а не что он согласен с -O3, — то есть не проверять ничего.
    if [ "$golden" != "physics-state-hash = 0xf238d0bc34325db6" ]; then
        echo "Debug разошёлся с Release по голдену физики: '$golden'"
        rc=1
    fi
    # Траектория персонажа (гейт 1 спеки #16) — второй голден на этом пути, и сверяется он тем же
    # способом: свой литерал, а не «хеш не пуст».
    if [ "$traj" != "character trajectory hash = 0x7801049c168bb301" ]; then
        echo "Debug разошёлся с Release по голдену траектории: '$traj'"
        rc=1
    fi
    # Третий голден этого пути — байты испечённого профиля: разбор числа из текста идёт по int64,
    # то есть ровно там, где расхождение уровней оптимизации означало бы UB, а не погрешность.
    if [ "$baked" != "hash 0xa554e52327f8d33d" ]; then
        echo "Debug разошёлся с Release по голдену таблицы профиля: '$baked'"
        rc=1
    fi
    # Четвёртый — свёртка ответов запросов к тайловой сетке: доли и точки считаются той же
    # арифметикой fix32, и расхождение уровней оптимизации на них тоже было бы UB.
    if [ "$tiles" != "tilemap answers hash = 0xb19419157787b1c3" ]; then
        echo "Debug разошёлся с Release по голдену ответов тайлмапа: '$tiles'"
        rc=1
    fi
    # Пятый — байты испечённой таблицы карт: собираются они тем же разбором числа по int64, что и
    # таблица профиля выше, и по тому же основанию обязаны совпасть между -O0 и -O3.
    if [ "$maps" != "hash 0xcd034f07f57b5f8b" ]; then
        echo "Debug разошёлся с Release по голдену таблицы карт: '$maps'"
        rc=1
    fi
    # Шестой — маршрут образца-платформера. Он единственный здесь сшивает приёмы в одну траекторию,
    # поэтому и сверяется отдельным литералом: «хеш не пуст» пропустило бы расхождение целиком.
    if [ "$plat" != "platformer trajectory hash = 0xfead7a87477a9258" ]; then
        echo "Debug разошёлся с Release по голдену образца-платформера: '$plat'"
        rc=1
    fi
    # Седьмой — анимационное состояние (гейт 1 спеки #17). Шкала клипа берётся делением с остатком
    # по uint64, а перескок кадров при скорости выше единицы — разностью порядковых номеров шага:
    # расхождение уровней оптимизации здесь было бы UB, а не «другой кадр».
    if [ "$anim" != "anim-state hash = 0x8db6e6730e0c446f" ]; then
        echo "Debug разошёлся с Release по голдену анимационного состояния: '$anim'"
        rc=1
    fi
    # Восьмой и девятый — траектория камеры (гейт 5 спеки #17). Каналов два, и оба здесь: потолок
    # скорости считает длину целочисленным корнем, привязка к пикселю делит с ОКРУГЛЕНИЕМ ВНИЗ по
    # int64, тряска умножает амплитуду на затухание тем же int64. Центр состояния не знает про две
    # последние вовсе — одним каналом расхождение -O0 с -O3 в них прошло бы молча.
    if [ "$cpath" != "camera-path hash = 0x15028037bdca9f22" ]; then
        echo "Debug разошёлся с Release по голдену траектории камеры: '$cpath'"
        rc=1
    fi
    # Десятый — байты нарезки атласа (шаг D спеки #17): привязка разбирается из десятичной записи
    # в Q16.16 умножением и делением по int64, то есть тем же местом, что таблицы профиля и карт.
    if [ "$atlas" != "hash 0xa9b9b74a0152af52" ]; then
        echo "Debug разошёлся с Release по голдену нарезки атласа: '$atlas'"
        rc=1
    fi
    if [ "$cview" != "camera-view hash = 0xd6c2f15de2ab50f8" ]; then
        echo "Debug разошёлся с Release по голдену вида камеры: '$cview'"
        rc=1
    fi
    # Одиннадцатый — поток квадов отладочного оверлея (шаг E спеки #17): половина стороны берётся
    # умножением на 1/2 в Q16.16, ширина глифа — дробью 9/25 от кегля, курсор текста копит шаг
    # сложением. Все три — арифметика по int64, то есть то же основание, что у десяти выше.
    if [ "$overlay" != "debug-overlay hash = 0x922ccf9cf4b0aecc" ]; then
        echo "Debug разошёлся с Release по голдену отладочного оверлея: '$overlay'"
        rc=1
    fi
    # Двенадцатый — порядок отрисовки и батчи (вертикаль 2, шаг A): ключ сортировки собран сдвигами
    # по int64, полуразмеры 9-slice — умножением на 1/2 в Q16.16. То же основание, что у одиннадцати
    # выше.
    if [ "$batch" != "sprite-batch hash = 0xbda7b692a87b1155" ]; then
        echo "Debug разошёлся с Release по голдену порядка отрисовки: '$batch'"
        rc=1
    fi
    return $rc
}

debug_golden
exit $?
