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
# смотрела `--limit 1`, то есть один прогон из двух. Итогом она напечатала `timeout` при двух
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

# Строки «<status> <conclusion> <id> <workflow>». Разбор через `cut`, а не `jq`: на Windows шелл —
# git-bash, и наличия jq там никто не обещал (то же правило, что у pct_of_budget в perf_sweep_lib).
fetch() {
    gh run list --commit "$SHA" --limit 20 \
        --json status,conclusion,workflowName,databaseId \
        --jq '.[] | "\(.status) \(.conclusion) \(.databaseId) \(.workflowName)"' 2>&1
}

report_jobs() {
    local status conclusion id name
    while read -r status conclusion id name; do
        [ -n "${status:-}" ] || continue
        printf '— %s (%s %s)\n' "$name" "$status" "$conclusion"
        gh run view "$id" --json jobs --jq '.jobs[] | "  \(.conclusion) \(.name)"' 2>&1 ||
            printf '  (job'"'"'ы этого прогона запросить не удалось)\n'
    done
}

say "коммит $(git rev-parse --short "$SHA" 2>/dev/null || printf '%s' "$SHA"), жду до $((TIMEOUT_S / 60)) мин"
errors=0
last="none"
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
    last=$(printf '%s\n' "$raw" | cut -d' ' -f1,2 | runs_verdict "$CI_WATCH_EXPECT_RUNS")
    case "$last" in
        success | failure)
            printf '%s\n' "$raw" | report_jobs
            say "вердикт: $last (прогонов $(printf '%s\n' "$raw" | grep -c .))"
            exit "$(verdict_exit "$last")"
            ;;
    esac
    sleep "$INTERVAL_S"
done

# Время вышло. Это ТОЖЕ «не знаю», и печатается оно словами, а не одиноким `timeout`: названо
# последнее НАБЛЮДЁННОЕ состояние, чтобы читающий не достраивал вердикт сам.
say "УЗНАТЬ НЕ УДАЛОСЬ — за $((TIMEOUT_S / 60)) мин прогоны не досчитали. Последнее состояние: $last"
say "Это НЕ значит «прогон красный»: проверь руками — gh run list --commit $SHA"
exit "$(verdict_exit unknown)"
