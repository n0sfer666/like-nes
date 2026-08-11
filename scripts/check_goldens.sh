#!/usr/bin/env bash
# Продукты сборки против ЛИТЕРАЛОВ: три проверки, у каждой свой вопрос.
#
#   debug   — голден физики совпадает в Debug и в Release (инвариант 1 спеки #15);
#   core    — восемь голденов ядра целы (гейт 2 спеки #11);
#   bundle  — `game.bundle` сверен с исходниками по секциям без внешних инструментов.
#
# Зовётся по ОДНОМУ имени (`check_goldens.sh debug|core|bundle`), как `tree_invariants.sh`: имя
# упавшей проверки обязано быть в логе, иначе «что-то из трёх разошлось» приходится доразбирать
# руками. `all` гоняет все три и не даёт первой упавшей скрыть остальные — то же основание, что у
# этапов `preflight.sh`.
#
# `build-ci` и `build-full` здесь не создаются: их делают этапы preflight, и промах каталога поэтому
# обязан быть слышен — отсутствие бинаря в `core`/`bundle` ошибка, а не пропуск. Исключение одно и
# своё: `debug` конфигурирует и собирает `build-debug` сам, потому что каталог под -O0 больше никому
# не нужен, а держать его на стороне вызывающего значило бы разнести вопрос и его условие.
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
STATE_TARGETS="framework_physics_test framework_physics_point_test framework_physics_sat_test framework_physics_gap_test framework_physics_order_test framework_physics_range_test framework_physics_clamp_test framework_physics_terms_test framework_physics_rt_test framework_physics_perf_test framework_physics_query_test framework_physics_overlap_test framework_physics_filter_test framework_physics_event_test framework_physics_stack_test framework_physics_sleep_test framework_physics_band_test framework_physics_wake_test framework_trig_test"

# Расхождение между уровнями оптимизации в целочисленной арифметике — это UB, а не «погрешность»,
# и локально оно проверяемо целиком: ни раннера, ни железа тут не нужно.
debug_golden() {
    cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DAUDIO_MINIAUDIO=OFF -DPLUGIN_UI=OFF -DPLUGIN_WASM=OFF >/dev/null || return 1
    BUILD_DIR=build-debug LIKE_NES_BUILD_SUBSET=1 LIKE_NES_BUILD_TYPE=Debug \
        LIKE_NES_BUILD_TARGETS="$STATE_TARGETS" bash scripts/build_check.sh || return 1
    local t bin out rc=0 golden=""
    for t in $STATE_TARGETS; do
        bin=$(find build-debug -maxdepth 2 -type f -name "$t" | head -1)
        if [ -z "$bin" ]; then echo "$t не собран"; rc=1; continue; fi
        out=$("$bin") || rc=1
        printf '%s\n' "$out" | grep -q ": PASS" || { printf '%s\n' "$out"; rc=1; }
        if [ "$t" = framework_physics_test ]; then
            golden=$(printf '%s\n' "$out" | grep -o 'physics-state-hash = 0x[0-9a-f]*')
        fi
    done
    # Тот же литерал, что у Release-шага в CI. Сверять Debug сам с собой значило бы проверять, что
    # -O0 воспроизводим, а не что он согласен с -O3, — то есть не проверять ничего.
    if [ "$golden" != "physics-state-hash = 0xbeb8e8bee1bba145" ]; then
        echo "Debug разошёлся с Release по голдену физики: '$golden'"
        rc=1
    fi
    return $rc
}

check_hash() { # имя-цели ожидаемый-хеш [аргументы]
    local bin name="$1" want="$2" out rc
    shift 2
    bin=$(find build-full -maxdepth 2 -type f -name "$name" | head -1)
    if [ -z "$bin" ]; then echo "$name не собран"; return 1; fi
    out=$("$bin" "$@" 2>&1)
    rc=$?
    # Статус спрашивается НАРАВНЕ с хешем, и вторым его не назовёшь. Голден печатается раньше
    # остальных утверждений бинаря, поэтому прогон, упавший на любом из них, всё равно оставляет
    # в выводе нужную строку: грепом одним этап читал бы «хеш сошёлся» как «тест прошёл», а это
    # разные утверждения. Прогон при этом ОДИН — второй ради диагностики мог бы напечатать не то,
    # что было отбито.
    if [ $rc -eq 0 ] && printf '%s\n' "$out" | grep -q "$want"; then
        echo "  $name = $want"
        return 0
    fi
    echo "  $name НЕ ЗАКРЫТ (код возврата $rc, ожидался $want):"
    # Шестнадцатеричный литерал в фильтре наравне со словами: `plugin_determinism_test` печатает
    # своё число строкой `H_with = 0x…`, где нет ни `hash`, ни `golden`, — и на нём диагностика
    # выходила ПУСТОЙ. Красный этап, не назвавший фактическое значение, заставляет лезть в лог
    # руками ровно тогда, когда он и должен был избавить от этого.
    printf '%s\n' "$out" | grep -iE 'hash|golden|fail|0x[0-9a-f]{8}' | sed 's/^/    /'
    return 1
}

# Восемь голденов гейта 2 спеки #11 — ЦЕЛИКОМ, а не только физика. Проверка заведена по конкретному
# промаху: правка округления в `fix32::operator*` (ядро, `engine/core/fixed.hpp`) сдвинула голден
# ядра И голден ввода, а локально не сработало ничего — физический голден-то сошёлся. Красное на
# трёх ОС за правку в общей арифметике — самый дорогой круг из тех, что этот файл существует, чтобы
# закрывать, и удивительным он был ровно потому, что арифметику правят не в той подсистеме, где
# краснеет.
core_goldens() {
    local rc=0 tmp grav wind
    check_hash determinism_test 0x6c4b121dbb47d13b || rc=1
    check_hash input_determinism_test 0xcc26a1897a326f6f || rc=1
    check_hash audio_golden 0x2cf5b5597afa3241 || rc=1
    check_hash scene_roundtrip_test 0x2de54a36e54e0684 || rc=1
    check_hash achievements_test 0xe728fef199e87fc9 || rc=1
    check_hash game_sim_test 0x32a094e89eacf2f2 || rc=1
    # Пекарь пишет бандл в текущий каталог, поэтому зовётся из временного: рабочее дерево обязано
    # остаться чистым — гейт 4 спеки #11 требует, чтобы локальный прогон не порождал `git status`.
    tmp=$(mktemp -d) || return 1
    ( cd "$tmp" && "$ROOT/build-full/assetc" --synthetic syn.bundle 2>&1 ) |
        grep -q 0xf2255dc74fbdb6bc && echo "  assetc --synthetic = 0xf2255dc74fbdb6bc" || {
        echo "  assetc --synthetic РАЗОШЁЛСЯ с 0xf2255dc74fbdb6bc"
        rc=1
    }
    rm -rf "$tmp"
    # Восьмой голден даёт `plugin_determinism_test`, и он собирается ЗДЕСЬ. Прежняя редакция путала
    # его с `plugin_wasm_test` — той цели действительно нужен wasmtime C-API из `deps/`, но она
    # утверждает не голден, а РАВЕНСТВО native и WASM на нём же. Из-за подмены восьмой голден не
    # проверялся локально вовсе, и перепин `fix32::operator*` в раунде #15 доехал незамеченным до
    # CI, где покраснел разом на трёх ОС. Цена ошибки ровно та, ради которой проверка и заводилась.
    grav=$(find build-full -maxdepth 2 -type f \
        \( -name 'plugin_gravity.so' -o -name 'plugin_gravity.dylib' \) | head -1)
    wind=$(find build-full -maxdepth 2 -type f \
        \( -name 'plugin_wind.so' -o -name 'plugin_wind.dylib' \) | head -1)
    if [ -n "$grav" ] && [ -n "$wind" ]; then
        check_hash plugin_determinism_test 0x7d9a6e60cbed4156 "$grav" "$wind" || rc=1
    else
        echo "  plugin_gravity/plugin_wind не собраны"
        rc=1
    fi
    # А вот это остаётся за CI, и названо вслух намеренно: «восемь голденов целы» и «native сошёлся
    # с WASM» — разные утверждения, и второе локально не проверяется.
    echo "  native==WASM (plugin_wasm_test) — только в CI: нужен wasmtime C-API из deps/"
    return $rc
}

# `game.bundle` лежит в git готовым, и перепечь его ЦЕЛИКОМ негде, кроме машины владельца:
# текстурная секция тянет tint и basisu. Секции, что пекутся чистыми парсерами, сверяются везде —
# без этого правка input.txt или achievements.txt без перепекания доезжает молча, а на бандле стоят
# и игра-образец, и sim-golden.
# Код возврата не различает «всё сошлось» и «сверять было нечего»: выпади секция из списка SECTIONS,
# бинарь честно напечатает меньшее число и вернёт ноль. Ассертится строка вместе с числом — тот же
# критерий, что в шаге CI, чтобы локально и на раннере отбивалось одно и то же.
verify_bundle() {
    local out
    out=$(build-ci/assetc --verify-game example_ugly_game/assets example_ugly_game/assets/game.bundle) || {
        printf '%s\n' "$out"
        return 1
    }
    printf '%s\n' "$out"
    case $out in
        *"verify: PASS - 2 tool-free section(s)"*) return 0 ;;
        *) echo "сверено не то число секций — список SECTIONS разошёлся с гейтом"; return 1 ;;
    esac
}

case ${1:-all} in
    debug)  debug_golden ;;
    core)   core_goldens ;;
    bundle) verify_bundle ;;
    all)
        rc=0
        debug_golden || rc=1
        core_goldens || rc=1
        verify_bundle || rc=1
        exit $rc
        ;;
    *)
        echo "использование: $0 debug|core|bundle|all" >&2
        exit 2
        ;;
esac
