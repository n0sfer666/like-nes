#!/usr/bin/env bash
# Продукты сборки против ЛИТЕРАЛОВ: три проверки, у каждой свой вопрос.
#
#   debug   — голден физики совпадает в Debug и в Release (инвариант 1 спеки #15);
#   core    — восемь голденов ядра целы (гейт 2 спеки #11);
#   bundle  — `game.bundle` сверен с исходниками по секциям без внешних инструментов.
# Зовётся по ОДНОМУ имени (`check_goldens.sh debug|core|bundle`), как `tree_invariants.sh`: имя
# упавшей проверки обязано быть в логе, иначе «что-то из трёх разошлось» приходится доразбирать
# руками. `all` гоняет все три и не даёт первой упавшей скрыть остальные — то же основание, что у
# этапов `preflight.sh`.
#
# `build-ci` и `build-full` здесь не создаются: их делают этапы preflight, и промах каталога поэтому
# обязан быть слышен — отсутствие бинаря в `core`/`bundle` ошибка, а не пропуск. Исключение одно и
# своё: `debug` конфигурирует и собирает `build-debug` сам, потому что каталог под -O0 больше никому
# не нужен, а держать его на стороне вызывающего значило бы разнести вопрос и его условие. По этой
# же границе он и вынесен в `check_debug_golden.sh` — вместе со списком целей, который CI сверяет
# маркером: две проверки читают чужие сборки, третья делает свою.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

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

# `game.bundle` лежит в git готовым, и перепечь его ЦЕЛИКОМ негде, кроме машины владельца: текстурная
# секция тянет tint и basisu. Секции, что пекутся чистыми парсерами, сверяются везде — без этого
# правка input.txt, achievements.txt или movement.txt без перепекания доезжает молча, а на бандле
# стоят и игра-образец, и sim-golden.
# Код возврата не различает «всё сошлось» и «сверять было нечего»: выпади секция из SECTIONS, бинарь
# напечатает меньшее число и вернёт ноль. Поэтому ассертится строка вместе с числом — тем же
# критерием, что в шаге CI.
verify_bundle() {
    local out
    out=$(build-ci/assetc --verify-game example_ugly_game/assets example_ugly_game/assets/game.bundle) || {
        printf '%s\n' "$out"
        return 1
    }
    printf '%s\n' "$out"
    case $out in
        *"verify: PASS - 5 tool-free section(s)"*) ;;
        *) echo "сверено не то число секций — список SECTIONS разошёлся с гейтом"; return 1 ;;
    esac
    # Второе утверждение о том же бандле, первым не заменяемое: `--verify-game` сверяет БАЙТЫ секции
    # с перепечённым текстом («бандл отстал от исходника») и ЧИТАТЕЛЯ не запускает вовсе, а эти тесты
    # читают секцию читателем и сверяют её с независимым ожиданием — `default_profile()` для профиля,
    # разбор `tilemap.txt` для карты, `parse_atlas_file` для нарезки. Проверено сломанной
    # реализацией: индексация флагов `x*h+y` краснит тест карты и оставляет `--verify-game` зелёным.
    # Образец-платформер стоит среди них со своим утверждением: сверки выше судят ОДНУ секцию
    # каждая, а он ПРОХОДИТ по уровню тем же контроллером, что и живая игра, — то есть ловит бандл,
    # где обе секции по отдельности верны, а вместе уровень непроходим.
    # Последним — анти-дрейф нарезки: у игры-образца регионы объявлены ДВАЖДЫ (`atlas.txt` в бандле
    # и `game::set_regions()` в коде), и ни одна из сверок выше про их расхождение не знает — они
    # обе судят бандл, а второй источник живёт в C++.
    local bin rc t all=0
    for t in framework_character_bundle_test framework_tilemap_bundle_test game_platformer_sim_test \
        framework_graphics_atlas_bundle_test game_atlas_regions_test; do
        bin=$(find build-ci build-full -maxdepth 2 -type f -name "$t" 2>/dev/null | head -1)
        if [ -z "$bin" ]; then echo "$t не собран"; all=1; continue; fi
        out=$("$bin" example_ugly_game/assets/game.bundle 2>&1); rc=$?
        printf '%s\n' "$out"
        { [ $rc -eq 0 ] && printf '%s\n' "$out" | grep -q ": PASS"; } || all=1   # статус наравне с грепом
    done
    return $all
}

case ${1:-all} in
    debug)  bash scripts/check_debug_golden.sh ;;
    core)   core_goldens ;;
    bundle) verify_bundle ;;
    all)
        rc=0
        bash scripts/check_debug_golden.sh || rc=1
        core_goldens || rc=1
        verify_bundle || rc=1
        exit $rc
        ;;
    *)
        echo "использование: $0 debug|core|bundle|all" >&2
        exit 2
        ;;
esac
