#!/usr/bin/env bash
# Ждёт, пока GitHub досчитает ВСЕ прогоны коммита, и печатает их job'ы построчно.
#
# Запуск:
#   bash scripts/ci_watch.sh                 # прогоны HEAD
#   bash scripts/ci_watch.sh <sha>           # прогоны конкретного коммита
#   CI_WATCH_TIMEOUT_S=600 bash scripts/ci_watch.sh
#
# Коды возврата: 0 — все прогоны зелёные, 1 — хоть один красный, 2 — УЗНАТЬ НЕ УДАЛОСЬ. Третий
# заведён потому, что предыдущая версия этого наблюдателя — инлайновый цикл в шелле — соврала
# 2026-08-22 тремя способами разом: `set -- $s` под zsh не разбивает строку на слова, поэтому
# совпадения со статусом не случалось НИКОГДА; сбой запроса она глотала и считала «ещё идёт»;
# смотрела `--limit 1`, то есть один прогон из нескольких. Итогом она напечатала `timeout` при двух
# зелёных прогонах часовой давности. Отсюда и файл: вызов через `bash <файл>` снимает капкан zsh
# по устройству, а не по дисциплине.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
. "$ROOT/scripts/ci_watch_lib.sh"

# Коммит разворачивается в ПОЛНЫЙ sha, и это не косметика: `gh run list --commit` сравнивает строку
# как есть, поэтому короткий `7cc3ba9` находит НОЛЬ прогонов и молча выглядит как «прогонов нет».
# Ровно тот класс, который ci_lint.py ловит правилом vacuous-gate: фильтр, не совпадающий ни с чем,
# неотличим от честного пустого ответа. Найдено живым прогоном, а не рассуждением.
SHA=$(git rev-parse "${1:-HEAD}" 2>/dev/null) || {
    printf 'ci-watch: «%s» — не коммит этого репозитория\n' "${1:-HEAD}" >&2
    exit 2
}
INTERVAL_S=${CI_WATCH_INTERVAL_S:-30}
TIMEOUT_S=${CI_WATCH_TIMEOUT_S:-2400}
# Запрос к api.github.com падает с `EOF` сам по себе: 22 августа так кончился один вызов из шести,
# следующие пять прошли. Один сбой — это сеть, пять подряд — это «узнать нельзя», и разница между
# ними обязана быть видна в коде возврата, а не только в логе.
MAX_ERRORS=${CI_WATCH_MAX_ERRORS:-5}

say() { printf 'ci-watch: %s\n' "$*"; }

# Строки «<status> <conclusion> <event> <id> <workflow>». Разбор через `cut`, а не `jq`: на Windows
# шелл — git-bash, и наличия jq там никто не обещал (то же правило, что у pct_of_budget в
# perf_sweep_lib). `event` тянется вместе с остальным, потому что именно он говорит, СКОЛЬКО
# прогонов ждать: число выводится из триггеров workflow'ов для тех событий, которые видны в ответе.
fetch() {
    gh run list --commit "$SHA" --limit 20 \
        --json status,conclusion,event,workflowName,databaseId \
        --jq '.[] | "\(.status) \(.conclusion) \(.event) \(.databaseId) \(.workflowName)"' 2>&1
}

report_jobs() {
    local status conclusion event id name
    while read -r status conclusion event id name; do
        [ -n "${status:-}" ] || continue
        printf '— %s (%s %s, %s)\n' "$name" "$status" "$conclusion" "$event"
        gh run view "$id" --json jobs --jq '.jobs[] | "  \(.conclusion) \(.name)"' 2>&1 ||
            printf '  (job'"'"'ы этого прогона запросить не удалось)\n'
    done
}

# Сколько прогонов обязано быть, если в ответе видны события `$1` (по одному в строке). Событий может
# быть несколько разом: push в ветку с открытым PR даёт и `push`, и `pull_request`, а число прогонов
# у них разное. Печатает пусто и возвращает 1, если триггеры разобрать не удалось.
expected_for() {
    local total=0 ev n
    while read -r ev; do
        [ -n "$ev" ] || continue
        n=$(expected_runs "$ev") || return 1
        total=$((total + n))
    done
    printf '%s' "$total"
}

say "коммит $(git rev-parse --short "$SHA" 2>/dev/null || printf '%s' "$SHA"), жду до $((TIMEOUT_S / 60)) мин"
errors=0
last="none"
prev_ids=""
SECONDS=0
while [ "$SECONDS" -lt "$TIMEOUT_S" ]; do
    if ! raw=$(fetch); then
        errors=$((errors + 1))
        say "запрос к GitHub упал ($errors из $MAX_ERRORS): ${raw%%$'\n'*}"
        if [ "$errors" -ge "$MAX_ERRORS" ]; then
            say "УЗНАТЬ НЕ УДАЛОСЬ — $MAX_ERRORS отказов подряд. Это НЕ значит «прогон красный»"
            exit "$(verdict_exit unknown)"
        fi
        sleep "$INTERVAL_S"
        continue
    fi
    errors=0
    if [ -n "$CI_WATCH_EXPECT_RUNS" ]; then
        expect=$CI_WATCH_EXPECT_RUNS
    elif ! expect=$(printf '%s\n' "$raw" | cut -d' ' -f3 | sort -u | expected_for); then
        say "УЗНАТЬ НЕ УДАЛОСЬ — триггеры в .github/workflows разобрать не вышло"
        say "Число прогонов можно назвать руками: CI_WATCH_EXPECT_RUNS=N bash scripts/ci_watch.sh"
        exit "$(verdict_exit unknown)"
    fi
    last=$(printf '%s\n' "$raw" | cut -d' ' -f1,2 | runs_verdict "$expect")
    ids=$(printf '%s\n' "$raw" | cut -d' ' -f4 | sort | tr '\n' ' ')
    case "$last" in
        failure)
            printf '%s\n' "$raw" | report_jobs
            say "вердикт: failure (прогонов $(printf '%s\n' "$raw" | grep -c .) из $expect)"
            exit "$(verdict_exit failure)"
            ;;
        success)
            # Зелёное подтверждается ВТОРЫМ опросом с тем же набором прогонов. Число, выведенное из
            # событий, знает про уже зарегистрированные прогоны — и ничего не знает про те, что
            # зарегистрируются через секунду: push в ветку с открытым PR порождает прогоны двух
            # событий, и между ними есть щель, в которой «все, кого вижу, зелёные» — правда про
            # половину набора. Отсюда цена в один интервал на каждом зелёном исходе.
            if [ "$ids" = "$prev_ids" ]; then
                printf '%s\n' "$raw" | report_jobs
                say "вердикт: success (прогонов $(printf '%s\n' "$raw" | grep -c .), ждали $expect)"
                exit "$(verdict_exit success)"
            fi
            say "все видимые прогоны зелёные, подтверждаю набор ещё одним опросом"
            ;;
    esac
    prev_ids=$ids
    sleep "$INTERVAL_S"
done

# Время вышло. Это ТОЖЕ «не знаю», и печатается оно словами, а не одиноким `timeout`: названо
# последнее НАБЛЮДЁННОЕ состояние, чтобы читающий не достраивал вердикт сам.
say "УЗНАТЬ НЕ УДАЛОСЬ — за $((TIMEOUT_S / 60)) мин прогоны не досчитали. Последнее состояние: $last"
say "Это НЕ значит «прогон красный»: проверь руками — gh run list --commit $SHA"
exit "$(verdict_exit unknown)"
