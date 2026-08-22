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

if [ "$fails" -ne 0 ]; then
    printf '\nci-watch-selftest: FAIL — проверок с находками %d\n' "$fails"
    exit 1
fi
printf '\nci-watch-selftest: PASS\n'
