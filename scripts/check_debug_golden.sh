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
STATE_TARGETS="framework_physics_test framework_physics_point_test framework_physics_sat_test framework_physics_gap_test framework_physics_order_test framework_physics_range_test framework_physics_clamp_test framework_physics_terms_test framework_physics_rt_test framework_physics_perf_test framework_physics_query_test framework_physics_corner_test framework_tilemap_test framework_tilemap_seam_test framework_physics_overlap_test framework_physics_index_test framework_physics_filter_test framework_physics_event_test framework_physics_stack_test framework_physics_impact_test framework_physics_sleep_test framework_physics_band_test framework_physics_wake_test framework_physics_handle_test framework_physics_depth_test framework_trig_test framework_character_test framework_character_window_test framework_character_jump_test framework_character_tunnel_test framework_character_collision_test framework_character_corner_test framework_character_snap_test framework_character_profile_test framework_character_profile_refusal_test framework_tilemap_bake_test framework_tilemap_refusal_test"

# Расхождение между уровнями оптимизации в целочисленной арифметике — это UB, а не «погрешность»,
# и локально оно проверяемо целиком: ни раннера, ни железа тут не нужно.
debug_golden() {
    cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null || return 1
    BUILD_DIR=build-debug LIKE_NES_BUILD_SUBSET=1 LIKE_NES_BUILD_TYPE=Debug \
        LIKE_NES_BUILD_TARGETS="$STATE_TARGETS" bash scripts/build_check.sh || return 1
    local t bin out rc=0 golden="" traj="" baked="" tiles="" maps=""
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
    if [ "$baked" != "hash 0x6fc67dbed5d4c5e9" ]; then
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
    return $rc
}

debug_golden
exit $?
