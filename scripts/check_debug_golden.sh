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
STATE_TARGETS="framework_physics_test framework_physics_point_test framework_physics_sat_test framework_physics_gap_test framework_physics_order_test framework_physics_range_test framework_physics_clamp_test framework_physics_terms_test framework_physics_rt_test framework_physics_perf_test framework_physics_query_test framework_physics_corner_test framework_tilemap_test framework_tilemap_seam_test framework_physics_overlap_test framework_physics_index_test framework_physics_filter_test framework_physics_event_test framework_physics_stack_test framework_physics_impact_test framework_physics_sleep_test framework_physics_band_test framework_physics_wake_test framework_physics_handle_test framework_physics_depth_test framework_physics_snapshot_test framework_physics_snapshot_islands_test framework_ids_test framework_rollback_plan_test framework_rollback_test framework_replay_test framework_brawl_test framework_brawl_floor_test framework_brawl_snapshot_test framework_brawl_seat_test framework_brawl_hotseat_test framework_brawl_crossplay_test framework_brawl_fighter_test framework_brawl_fighter_refusal_test framework_brawl_hit_test framework_brawl_hit_rules_test framework_brawl_immune_test framework_brawl_strike_test framework_brawl_chain_test framework_brawl_react_test framework_brawl_run_test framework_brawl_guard_test framework_brawl_dodge_test framework_brawl_dodge_flick_test framework_flick_test framework_action_buffer_test framework_brawl_grab_test framework_brawl_clash_test framework_brawl_throw_test framework_brawl_camera_test framework_brawl_stage_test framework_brawl_wave_test framework_brawl_wave_lock_test framework_brawl_section_test framework_brawl_platform_test framework_brawl_scroll_test framework_brawl_settle_test framework_brawl_leash_test framework_brawl_progress_test framework_brawl_progress_resim_test framework_brawl_movers_test framework_brawl_path_follow_test framework_brawl_conveyor_test framework_brawl_conveyor_platform_test framework_brawl_rope_test framework_brawl_rope_ride_test framework_brawl_rope_grab_test framework_brawl_rope_world_test framework_brawl_projectile_test framework_brawl_projectile_edge_test framework_brawl_prop_test framework_ai_test framework_ai_resim_test brawl_resim_bench framework_trig_test framework_character_test framework_character_window_test framework_character_jump_test framework_character_tunnel_test framework_character_collision_test framework_character_corner_test framework_character_snap_test framework_character_profile_test framework_character_profile_refusal_test framework_tilemap_bake_test framework_tilemap_refusal_test framework_character_slope_test framework_character_oneway_test framework_character_platform_test framework_character_push_test framework_character_perf_test framework_tilemap_slope_test framework_tilemap_oneway_test framework_tilemap_ladder_test framework_character_ladder_test game_platformer_sim_test game_platformer_rollback_test game_platformer_replay_test framework_graphics_clip_test framework_graphics_event_test framework_graphics_anim_test framework_graphics_path_test framework_graphics_atlas_test framework_graphics_atlas_refusal_test framework_graphics_debug_test framework_graphics_batch_test framework_graphics_tile_test framework_graphics_particle_test framework_graphics_viewport_test"

# Расхождение между уровнями оптимизации в целочисленной арифметике — это UB, а не «погрешность»,
# и локально оно проверяемо целиком: ни раннера, ни железа тут не нужно.
GOLDENS=()
golden() { GOLDENS+=("$1|$2|$3"); }
# Тот же литерал, что у Release-шага в CI. Сверять Debug сам с собой значило бы проверять, что
# -O0 воспроизводим, а не что он согласен с -O3, — то есть не проверять ничего.
golden framework_physics_test "physics-state-hash = 0xf238d0bc34325db6" "физики"
# Траектория персонажа (гейт 1 спеки #16) — второй голден на этом пути, и сверяется он тем же
# способом: свой литерал, а не «хеш не пуст».
golden framework_character_test "character trajectory hash = 0x7801049c168bb301" "траектории"
# Третий голден этого пути — байты испечённого профиля: разбор числа из текста идёт по int64,
# то есть ровно там, где расхождение уровней оптимизации означало бы UB, а не погрешность.
golden framework_character_profile_test "hash 0xa554e52327f8d33d" "таблицы профиля"
golden framework_brawl_fighter_test "hash 0x34030b3c5a326d2d" "таблицы бойца"
golden framework_ai_resim_test "ai fight hash = 0x5138fba1d8279fa4" "боя с ИИ"
golden framework_brawl_progress_resim_test "progress course hash = 0xf1c00aba70879d31" "прогресса уровня"
golden brawl_resim_bench "resim bench fight hash = 0x0f60b3fbe989d27e" "бенча ресима"
golden framework_brawl_movers_test "movers lift hash = 0x7ec76882f27ca481" "лифта по пути"
golden framework_brawl_rope_test "rope ride hash = 0x03892e88165766d3" "троса"
# Четвёртый — свёртка ответов запросов к тайловой сетке: доли и точки считаются той же
# арифметикой fix32, и расхождение уровней оптимизации на них тоже было бы UB.
golden framework_tilemap_test "tilemap answers hash = 0xb19419157787b1c3" "ответов тайлмапа"
# Пятый — байты испечённой таблицы карт: собираются они тем же разбором числа по int64, что и
# таблица профиля выше, и по тому же основанию обязаны совпасть между -O0 и -O3.
golden framework_tilemap_bake_test "hash 0xcd034f07f57b5f8b" "таблицы карт"
# Шестой — маршрут образца-платформера. Он единственный здесь сшивает приёмы в одну траекторию,
# поэтому и сверяется отдельным литералом: «хеш не пуст» пропустило бы расхождение целиком.
golden game_platformer_sim_test "platformer trajectory hash = 0xfead7a87477a9258" "образца-платформера"
# Заявка последнего тика записанного реплея (гейт 6 спеки #22) — свёртка МАРКИ, а не траектории:
# окно прощения, запомненная опора, счётчики работы физики и хеш событий в голден выше не входят
# вовсе. Расхождение -O0 с -O3 в любом из этих полей было бы UB, и назвать себя ему больше негде:
# верификатор сравнивает заявку с заявкой ВНУТРИ одного прогона и на обоих уровнях зелен одинаково.
golden game_platformer_replay_test "platformer replay claim = 0x1ca0e823e931de86" "заявки реплея"
# Седьмой — анимационное состояние (гейт 1 спеки #17). Шкала клипа берётся делением с остатком
# по uint64, а перескок кадров при скорости выше единицы — разностью порядковых номеров шага:
# расхождение уровней оптимизации здесь было бы UB, а не «другой кадр».
golden framework_graphics_anim_test "anim-state hash = 0x8db6e6730e0c446f" "анимационного состояния"
# Восьмой и девятый — траектория камеры (гейт 5 спеки #17). Каналов два, и оба здесь: потолок
# скорости считает длину целочисленным корнем, привязка к пикселю делит с ОКРУГЛЕНИЕМ ВНИЗ по
# int64, тряска умножает амплитуду на затухание тем же int64. Центр состояния не знает про две
# последние вовсе — одним каналом расхождение -O0 с -O3 в них прошло бы молча.
golden framework_graphics_path_test "camera-path hash = 0x15028037bdca9f22" "траектории камеры"
# Десятый — байты нарезки атласа (шаг D спеки #17): привязка разбирается из десятичной записи
# в Q16.16 умножением и делением по int64, то есть тем же местом, что таблицы профиля и карт.
golden framework_graphics_atlas_test "hash 0xa9b9b74a0152af52" "нарезки атласа"
golden framework_graphics_path_test "camera-view hash = 0xd6c2f15de2ab50f8" "вида камеры"
# Одиннадцатый — поток квадов отладочного оверлея (шаг E спеки #17): половина стороны берётся
# умножением на 1/2 в Q16.16, ширина глифа — дробью 9/25 от кегля, курсор текста копит шаг
# сложением. Все три — арифметика по int64, то есть то же основание, что у десяти выше.
golden framework_graphics_debug_test "debug-overlay hash = 0x922ccf9cf4b0aecc" "отладочного оверлея"
# Двенадцатый — порядок отрисовки и батчи (вертикаль 2, шаг A): ключ сортировки собран сдвигами
# по int64, полуразмеры 9-slice — умножением на 1/2 в Q16.16. То же основание, что у одиннадцати
# выше.
golden framework_graphics_batch_test "sprite-batch hash = 0xbda7b692a87b1155" "порядка отрисовки"
# Двенадцатый — поток спрайтов тайловой карты (вертикаль 2, шаг B). Окно считается
# целочисленным делением с округлением ВНИЗ по сырому Q16.16 со знаком, а центр и полуразмер
# тайла — умножением на 1/2: расхождение -O0 с -O3 здесь было бы UB, а не «сдвинулась картинка».
golden framework_graphics_tile_test "tile-draw hash = 0x599044fa7aabfddc" "отрисовки тайлов"
# Тринадцатый — состояние эмиттеров частиц (гейт 4 спеки #17): направление через табличный
# `sin_turns`/`cos_turns` по Q16.16, затухание умножением на дробь, доля темпа — сложением.
golden framework_graphics_particle_test "particle-sim hash = 0xab81c90241aaa65a" "частиц"
# Четырнадцатый — развёртка зума (вертикаль 2, шаг D): половина видимой области считается
# делением в Q16.16, привязка к пикселю — делением с округлением ВНИЗ по int64 со знаком.
golden framework_graphics_viewport_test "viewport hash = 0x5bcea5ecad651b07" "зума"

debug_golden() {
    cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null || return 1
    BUILD_DIR=build-debug LIKE_NES_BUILD_SUBSET=1 LIKE_NES_BUILD_TYPE=Debug \
        LIKE_NES_BUILD_TARGETS="$STATE_TARGETS" bash scripts/build_check.sh || return 1
    local t bin out row target lit label got rc=0
    for row in "${GOLDENS[@]}"; do
        target=${row%%|*}
        case " $STATE_TARGETS " in
        *" $target "*) ;;
        *) echo "голден цели $target не сверяется: её нет в STATE_TARGETS"; rc=1 ;;
        esac
    done
    for t in $STATE_TARGETS; do
        bin=$(find build-debug -maxdepth 2 -type f \( -name "$t" -o -name "$t.exe" \) | head -1)
        if [ -z "$bin" ]; then echo "$t не собран"; rc=1; fi
        out=""
        if [ -n "$bin" ]; then
            out=$("$bin") || rc=1
            printf '%s\n' "$out" | grep -q ": PASS" || { printf '%s\n' "$out"; rc=1; }
        fi
        for row in "${GOLDENS[@]}"; do
            IFS='|' read -r target lit label <<<"$row"
            [ "$target" = "$t" ] || continue
            got=$(printf '%s\n' "$out" | grep -o "${lit%0x*}0x[0-9a-f]*")
            if [ "$got" != "$lit" ]; then
                echo "Debug разошёлся с Release по голдену $label: '$got'"
                rc=1
            fi
        done
    done
    return $rc
}

debug_golden
exit $?
