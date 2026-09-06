#!/usr/bin/env bash
# Позитивный контроль net_budget_verdict: до аудита #21 у гейта цены сетевого кадра не было НИ ОДНОЙ
# фикстуры, где он падает, — он печатал долю бюджета и всегда возвращал ноль. Утверждение, у которого
# нет такой фикстуры, неотличимо от отсутствующего.
#
# Ни сборки, ни пиров: вердикт есть чистая функция от строк замера — то же разделение, что у
# ci_watch_lib.sh, где суждение о прогоне проверяется фикстурами без единого запроса к сети.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
NAME=net-budget-selftest
# shellcheck source=scripts/owner_net_budget_lib.sh
. "$ROOT/scripts/owner_net_budget_lib.sh"
pass=0; mut=0; fail=0
FRAME=16.67

# Формат — тот, что печатает example_ugly_game/platformer_peer_budget.hpp: роль вторым полем с
# двоеточием, величина после `worst=`. Своя копия формата разъехалась бы с бинарём молча.
line() { printf '  peer %s: %s worst=%.3f ms mean=%.3f ms over 600 ticks\n' "$1" "$2" "$3" "$3"; }

expect() {
    local want="$1" needle="$2" what="$3" input="$4" out rc=0
    out=$(printf '%s' "$input" | net_budget_verdict "$FRAME" 2>&1) || rc=$?
    if [ "$want" = pass ]; then
        if [ "$rc" != 0 ]; then
            printf '%s: FAIL %s — ждали PASS, код %s\n%s\n' "$NAME" "$what" "$rc" "$out" >&2
            fail=$((fail + 1)); return
        fi
        pass=$((pass + 1))
    else
        if [ "$rc" = 0 ]; then
            printf '%s: FAIL %s — порча проехала зелёной:\n%s\n' "$NAME" "$what" "$out" >&2
            fail=$((fail + 1)); return
        fi
        if ! grep -qF -- "$needle" <<<"$out"; then
            printf '%s: FAIL %s — отказ не назвал «%s»:\n%s\n' "$NAME" "$what" "$needle" "$out" >&2
            fail=$((fail + 1)); return
        fi
        mut=$((mut + 1))
    fi
    printf '%s: OK   %s\n' "$NAME" "$what"
}

# Подстановка `$( … )` съедает хвостовой перевод строки, поэтому склейка вызовов дала бы ОДНУ строку
# на четыре замера: awk разобрал бы первый `worst=` и молчал о трёх остальных — фикстура проверяла бы
# собственную сборку, а не вердикт. Перевод дописывается явно.
fx() {
    local out=""
    while [ "$#" -ge 3 ]; do out+="$(line "$1" "$2" "$3")"$'\n'; shift 3; done
    printf '%s' "$out"
}

FIT=$(fx send sim 4.000 send net 2.000 recv sim 4.500 recv net 1.500)
# Ключевая фикстура вертикали: ПОРОЗНЬ каждая половина влезает в бюджет, а ВМЕСТЕ они его пробивают.
# Ровно ради этого случая сумма и заведена, и ровно его прежняя реализация печатала зелёным.
SUM_OVER=$(fx send sim 10.000 send net 8.000 recv sim 4.000 recv net 1.000)
HALF_OVER=$(fx send sim 20.000 send net 1.000 recv sim 4.000 recv net 1.000)
# Граница берётся слагаемым и нулём, а не двумя долями: 16.000 + 0.670 в двоичной плавучке даёт
# число БОЛЬШЕ 16.67, и опорный `pass` падал бы по причине арифметики, а не вердикта.
EDGE=$(fx send sim 16.670 send net 0.000)

expect pass '' 'обе роли укладываются в бюджет' "$FIT"
expect pass '' 'сумма ровно на границе бюджета превышением не считается' "$EDGE"
expect fail 'кадр пира send не влезает' 'сумма пробивает бюджет при обеих половинах внутри него' "$SUM_OVER"
expect fail 'кадр пира send не влезает' 'одна половина сама больше бюджета' "$HALF_OVER"
expect fail 'сравнивать с бюджетом нечего' 'пустой вход — отказ, а не «уложились»' ''
expect fail 'сравнивать с бюджетом нечего' 'вывод без строк замера — отказ' 'peer send: ready
peer recv: ready
'

# Сломанные реализации: набор, который их пропускает, выглядит ровно как честный. Подмена обязана
# РЕАЛЬНО менять функцию — иначе «утверждение её отбило» проверяло бы собственный промах.
subbed() {
    local what="$1" before after out rc=0
    before=$(declare -f net_budget_verdict | cksum)
    eval "$2"
    after=$(declare -f net_budget_verdict | cksum)
    [ "$before" != "$after" ] || {
        printf '%s: FAIL подмена не изменила net_budget_verdict (%s)\n' "$NAME" "$what" >&2
        fail=$((fail + 1))
    }
    # Сломанная обязана ОТДАТЬ НОЛЬ на фикстуре, где честная отдаёт единицу: этим и доказывается, что
    # кейс различает реализации. Ненулевой код у неё означал бы, что фикстура падает по чужой причине
    # и о дефекте не говорит ничего.
    out=$(printf '%s' "$SUM_OVER" | net_budget_verdict "$FRAME" 2>&1) || rc=$?
    if [ "$rc" = 0 ]; then
        printf '%s: OK   отбита сломанная реализация: %s\n' "$NAME" "$what"
        mut=$((mut + 1))
    else
        printf '%s: FAIL сломанная реализация (%s) упала кодом %s — фикстура судит не её дефект:\n%s\n' \
            "$NAME" "$what" "$rc" "$out" >&2
        fail=$((fail + 1))
    fi
    eval "$ORIGINAL"
}
ORIGINAL=$(declare -f net_budget_verdict)

# Та самая, что стояла на дереве до аудита #21: печатает проценты и уходит в `sort`, чей код и
# становится кодом пайпа.
subbed 'печатает долю бюджета, но не сравнивает с ним' '
net_budget_verdict() {
    awk -v frame="$1" '"'"'
        match($0, /worst=[0-9.]+/) { w = substr($0, RSTART + 6, RLENGTH - 6) + 0
            role = $2; sub(":", "", role); worst[role] += w }
        END { for (r in worst) printf "  худший кадр пира %s: %.1f%% бюджета\n", r, 100 * worst[r] / frame }
    '"'"' | sort
}'

# Половины порознь: каждая в бюджете, и гейт молчит о кадре, который вдвое его больше.
subbed 'судит половины порознь, а не их сумму' '
net_budget_verdict() {
    local frame="$1" out rc=0
    out=$(awk -v frame="$frame" '"'"'
        BEGIN { over = 0; seen = 0 }
        match($0, /worst=[0-9.]+/) { w = substr($0, RSTART + 6, RLENGTH - 6) + 0; seen++
            if (w > frame) { printf "  FAIL: половина кадра больше бюджета\n"; over = 1 } }
        END { if (seen == 0) exit 2; exit over }
    '"'"') || rc=$?
    [ -z "$out" ] || printf "%s\n" "$out"
    return $rc
}'

# Гейт обязан ЗВАТЬ функцию, а не носить свою копию арифметики: копия разъедется с этим набором
# молча, и он проверял бы то, чего на дереве нет. Комментарии снимаются строками — по тому же
# основанию, что в assert_owner_check_reads_doc.
code=$(sed '/^[[:space:]]*#/d' "$ROOT/scripts/owner_net_budget.sh")
if grep -q 'net_budget_verdict' <<<"$code" && grep -q 'owner_net_budget_lib.sh' <<<"$code"; then
    printf '%s: OK   гейт зовёт net_budget_verdict из библиотеки\n' "$NAME"
    pass=$((pass + 1))
else
    printf '%s: FAIL scripts/owner_net_budget.sh не зовёт net_budget_verdict — арифметика у него своя\n' "$NAME" >&2
    fail=$((fail + 1))
fi

printf '%s: %s — опорных pass %d, отбито порч %d\n' "$NAME" \
    "$( [ "$fail" = 0 ] && echo PASS || echo FAIL )" "$pass" "$mut"
[ "$fail" = 0 ]
