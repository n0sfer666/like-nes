#!/usr/bin/env bash
# Самопроверка суждения о прогонах CI — той же дисциплиной, что у perf_sweep_selftest.sh: правило,
# у которого нет сломанной фикстуры, ничего не сторожит.
#
# Отдельный файл, потому что живьём это доказать нечем: чтобы увидеть «один прогон красный, второй
# ещё идёт», нужно сломать CI по заказу и успеть спросить в окне между двумя job'ами. Фикстуры
# берутся из форм ответа, которые GitHub реально отдавал 22 августа.
set -uo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
. "$DIR/ci_watch_lib.sh"

fails=0

# expect | ожидаемый вердикт | прогоны через запятую «status:conclusion» | имя
FIXTURES='2|success|completed:success,completed:success|оба прогона досчитали успехом
2|running|completed:success,in_progress:|второй ещё идёт
2|failure|completed:failure,in_progress:|красный назван раньше незавершённого
2|failure|completed:success,completed:failure|красный второй не теряется за зелёным первым
2|none|EMPTY|прогонов нет вовсе — это находка, а не зелень
2|running|completed:success|один прогон из двух — гонка регистрации, не успех
1|success|completed:success|при ожидании одного прогона один и достаточен
2|failure|completed:success,completed:|пустой вывод у завершённого идёт в красное
2|success|completed:success,completed:skipped|пропущенный прогон успеху не мешает
2|failure|completed:success,completed:cancelled|отменённый прогон — не успех
2|running|queued:,queued:|оба в очереди
2|failure|completed:timed_out,completed:success|прогон по таймауту — не успех'

# Разворот записи фикстуры в тот же вид, в котором строки приходят из `gh run list | cut`.
feed() {
    local runs=$1
    [ "$runs" = "EMPTY" ] && return 0
    printf '%s\n' "${runs//,/$'\n'}" | tr ':' ' '
}

# Прогон всего набора через ЛЮБУЮ реализацию вердикта. Число расхождений возвращается КОДОМ, а не
# печатью: в громком режиме stdout занят строками отчёта, и счётчик, подмешанный туда же, читался бы
# вызывающим вместе с ними. Ровно на этом первая версия файла напечатала PASS поверх ошибки счёта.
run_fixtures() {
    local fn=$1 loud=$2 expect want runs name got bad=0
    while IFS='|' read -r expect want runs name; do
        [ -n "${expect:-}" ] || continue
        got=$(feed "$runs" | "$fn" "$expect")
        if [ "$got" = "$want" ]; then
            [ "$loud" = "loud" ] && printf 'ok   %s\n' "$name"
        else
            bad=$((bad + 1))
            [ "$loud" = "loud" ] &&
                printf 'FAIL %s: ожидалось «%s», получено «%s»\n' "$name" "$want" "$got"
        fi
    done <<<"$FIXTURES"
    return "$bad"
}

run_fixtures runs_verdict loud
fails=$((fails + $?))

# --- коды возврата ------------------------------------------------------------------------------
# «Не знаю» обязано иметь СВОЙ код: ровно на его отсутствии прошлый наблюдатель и соврал.
ok() {
    if [ "$2" = "$1" ]; then
        printf 'ok   %s\n' "$3"
    else
        printf 'FAIL %s: ожидалось «%s», получено «%s»\n' "$3" "$1" "$2"
        fails=$((fails + 1))
    fi
}
ok 0 "$(verdict_exit success)" "зелёный вердикт — код 0"
ok 1 "$(verdict_exit failure)" "красный вердикт — код 1"
ok 2 "$(verdict_exit running)" "незавершённый — код 2, а не 0 и не 1"
ok 2 "$(verdict_exit none)" "прогонов нет — код 2"
ok 2 "$(verdict_exit unknown)" "запрос не удался — код 2"

# --- позитивный контроль самих фикстур ----------------------------------------------------------
# Набор, который не отбивает сломанную реализацию, — декорация, и выглядит он ровно как честный.
# Три поломки, каждая — способ соврать, уже случившийся в прошлой версии наблюдателя.
always_green() {
    cat >/dev/null
    printf 'success'
}
ignores_count() {
    local expect=$1
    runs_verdict 1 <&0
    : "$expect"
}
red_loses_to_pending() {
    local expect=$1 n=0 pending=0 status conclusion
    while read -r status conclusion; do
        [ -n "${status:-}" ] || continue
        # Вывод прогона эта реализация читает и НЕ смотрит — в этом её поломка. Строчка ниже
        # существует, чтобы поломка была видна глазами, а не выглядела опечаткой разбора.
        : "$conclusion"
        n=$((n + 1))
        [ "$status" = "completed" ] || pending=1
    done
    if [ "$n" -eq 0 ]; then printf 'none'
    elif [ "$pending" -eq 1 ]; then printf 'running'
    elif [ "$n" -lt "$expect" ]; then printf 'running'
    else printf 'success'; fi
}
for broken in always_green ignores_count red_loses_to_pending; do
    run_fixtures "$broken" quiet
    caught=$?
    if [ "$caught" -gt 0 ]; then
        printf 'ok   сломанная реализация %s отбита (%s расхождений)\n' "$broken" "$caught"
    else
        printf 'FAIL фикстуры пропустили сломанную реализацию %s — набор ничего не сторожит\n' "$broken"
        fails=$((fails + 1))
    fi
done

# --- сколько прогонов ждать ---------------------------------------------------------------------
# Число прогонов раньше стояло константой `2` («на push идут ci и dco») и было неверным: `dco.yml`
# подписан только на `pull_request`. Проверяется поэтому не «сколько получилось», а РАЗБОР триггеров
# на фикстурах тех же трёх форм, что лежат в дереве.
WF_OK=$(mktemp -d)
WF_BAD=$(mktemp -d)
trap 'rm -rf "$WF_OK" "$WF_BAD"' EXIT

printf 'on:\n  push:\n  pull_request:\njobs:\n  a:\n    runs-on: ubuntu-latest\n' >"$WF_OK/ci.yml"
printf 'on:\n  pull_request:\n    types: [opened, synchronize]\n' >"$WF_OK/dco.yml"
printf "on:\n  push:\n    tags:\n      - 'v*'\n  workflow_dispatch:\n" >"$WF_OK/release.yml"
printf 'on: [push]\n' >"$WF_BAD/inline.yml"

ok 1 "$(expected_runs push "$WF_OK")" "push: тег-фильтр не делает workflow подписчиком push'а ветки"
ok 2 "$(expected_runs pull_request "$WF_OK")" "pull_request: подписаны оба, ci и dco"
ok 0 "$(expected_runs schedule "$WF_OK")" "событие без подписчиков — честный ноль, а не отказ"
if expected_runs push "$WF_BAD" >/dev/null 2>&1; then
    printf 'FAIL инлайновый «on: [push]» разобран числом — «не понял» выдано за ответ\n'
    fails=$((fails + 1))
else
    ok 1 1 "неразобранная форма «on:» отказывается числом, а не отдаёт молчаливый ноль"
fi

# Позитивный контроль на ЖИВОМ дереве: ровно то утверждение, на котором наблюдатель врал сорок
# минут. Разъедется дерево с этими числами — расходится не гейт, а знание о том, чего ждать.
ok 1 "$(expected_runs push "$DIR/../.github/workflows")" "в дереве на push подписан один workflow"
ok 2 "$(expected_runs pull_request "$DIR/../.github/workflows")" "а на pull_request — два"

# Сломанные реализации счёта. Первая — та самая константа, вторая — разбор, не заметивший фильтра
# по тегам: обе дают на pull_request правильную двойку и расходятся ровно на push.
counts_ok() {
    [ "$("$1" push "$WF_OK")" = 1 ] && [ "$("$1" pull_request "$WF_OK")" = 2 ]
}
constant_two() {
    : "$1" "$2"
    printf '2'
}
ignores_tag_filter() {
    local event=$1 dir=$2 total=0 f
    for f in "$dir"/*.yml; do
        grep -q "^  $event:" "$f" && total=$((total + 1))
    done
    printf '%s' "$total"
}
counts_ok expected_runs || {
    printf 'FAIL честный счёт по триггерам не проходит собственные фикстуры\n'
    fails=$((fails + 1))
}
for broken in constant_two ignores_tag_filter; do
    if counts_ok "$broken"; then
        printf 'FAIL фикстуры пропустили сломанный счёт %s — ждать он будет не тех прогонов\n' "$broken"
        fails=$((fails + 1))
    else
        printf 'ok   сломанный счёт %s отбит\n' "$broken"
    fi
done

# --- ожидание наблюдателя -----------------------------------------------------------------------
# Второй предмет — не вердикт, а ЦИКЛ во времени: сколько опросов сделано и смотрит ли наблюдатель
# ещё раз перед «не знаю». Он живёт своим файлом и зовётся внешней командой, а не подключается
# точкой: тогда его можно прогнать по одной, а в логе стоит имя упавшей самопроверки.
bash "$DIR/ci_watch_wait_selftest.sh" || fails=$((fails + 1))

if [ "$fails" -ne 0 ]; then
    printf '\nci-watch-selftest: FAIL — проверок с находками %d\n' "$fails"
    exit 1
fi
printf '\nci-watch-selftest: PASS\n'
