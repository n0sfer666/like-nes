#!/usr/bin/env bash
# Цена кадра СЕТЕВОГО прогона на этой машине: гейт 8 спеки #22.
#
# Мерит ТОТ ЖЕ бинарь, что ведёт запись прогона (`--peer` пишет `.replay` строкой за тик через
# `RecordingSim`): цифра со сборки без записи описывала бы не тот кадр, который живёт в вертикали 2.
#
# Пиры поднимаются НАПРЯМУЮ, а не прогоном `game_platformer_net_test`. Распорядитель отдаёт stdout
# детей в /dev/null (`Child::spawn`), то есть ровно те две строки, ради которых файл заведён, до
# отчёта не доезжают вовсе — из гейта осталось бы слово PASS про сходимость хешей.
#
# Внешней командой по тому же основанию, что `owner_perf.sh`: число владельцу нужно и само по себе,
# без часового прогона всего дерева. Печатает в СТДАУТ, в отчёт его заводит вызывающий.
#
# Запуск:
#   bash scripts/owner_net_budget.sh
#   BUILD_DIR=build-full bash scripts/owner_net_budget.sh
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1

BUILD_DIR=${BUILD_DIR:-build}
BUNDLE=${BUNDLE:-example_ugly_game/assets/game.bundle}
FRAME_MS=16.67

BIN="$BUILD_DIR/game_platformer_net_test"
[ -x "$BIN" ] || BIN="$BUILD_DIR/game_platformer_net_test.exe"
printf '\n--- Цена кадра сетевого прогона (game_platformer_net_test --peer, гейт 8 спеки #22)\n'
if [ ! -x "$BIN" ]; then
    printf '  game_platformer_net_test не собран — число на этой машине не снято\n'
    exit 0
fi
if [ ! -f "$BUNDLE" ]; then
    printf '  бандла нет: %s — число на этой машине не снято\n' "$BUNDLE"
    exit 0
fi

WORK=$(mktemp -d 2>/dev/null || mktemp -d -t likenesnet)
PREFIX="$WORK/peer"
"$BIN" --peer send "$BUNDLE" "$PREFIX" >"$WORK/send.out" 2>&1 &
send_pid=$!
"$BIN" --peer recv "$BUNDLE" "$PREFIX" >"$WORK/recv.out" 2>&1 &
recv_pid=$!
wait "$send_pid"; send_rc=$?
wait "$recv_pid"; recv_rc=$?

rc=0
cat "$WORK/send.out" "$WORK/recv.out"
if [ "$send_rc" -ne 0 ] || [ "$recv_rc" -ne 0 ]; then
    printf '  FAIL: пиры вышли ненулём: send=%d recv=%d (код 9 — замер не сошёлся с прогоном)\n' \
        "$send_rc" "$recv_rc"
    rc=1
fi

# Позитивный контроль: обе строки замера обязаны НАЙТИСЬ. Грепу, не нашедшему ничего, нечего
# сравнивать с бюджетом, и молчание арифметики ниже читалось бы как «уложились» — тот же вакуумный
# гейт, что ловит правило `vacuous-gate` в `ci_lint.py`.
for side in send recv; do
    for what in sim net; do
        grep -q "$what worst=" "$WORK/$side.out" || {
            printf '  FAIL: в выводе %s нет строки замера «%s worst=»\n' "$side" "$what"
            rc=1
        }
    done
done

# Судится СУММА худших: кадр пира — это шаг симуляции плюс обслуживание сокета, и порознь каждая
# половина влезает в бюджет даже тогда, когда вместе они его пробивают.
if [ "$rc" -eq 0 ]; then
    grep -h 'worst=' "$WORK/send.out" "$WORK/recv.out" |
        awk -v frame="$FRAME_MS" '
            match($0, /worst=[0-9.]+/) {
                w = substr($0, RSTART + 6, RLENGTH - 6) + 0
                role = $2; sub(":", "", role)
                worst[role] += w
            }
            END {
                for (r in worst)
                    printf "  худший кадр пира %s: %.3f мс из %.2f (%.1f%% бюджета)\n",
                        r, worst[r], frame, 100 * worst[r] / frame
            }' | sort
    printf '  бюджет 16.67 мс — тот же, что у гейта 8 спеки #15 и гейта 7 спеки #16\n'
    printf '  сон петли (1 мс, когда шагнуть нечем) в замер не входит: он не работа кадра\n'
fi

rm -rf "$WORK"
exit $rc
