#!/usr/bin/env bash
# Позитивный контроль гейта iOS-симулятора на ЖИВОМ прогоне: оболочка собирается с испорченным
# view.mm, и гейт обязан упасть по причине именно этой порчи. Фикстуры `ios_touch_verdict.py
# --selftest` доказывают правила разбора, но не то, что симулятор, синтезатор и проба вообще
# доносят до них дефект, — это доказывает только мутант, прошедший весь путь.
#
#   bash scripts/ios_sim_gate_live_selftest.sh   # ~2 минуты: два мутанта и чистый прогон
#
# Последним идёт чистый прогон: он же возвращает в кеш CMake настоящий view.mm.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/platform/ios/view.mm"
MUT="$ROOT/build-ios-xcode/mutants"
FAILS=0

[ "$(uname -s)" = Darwin ] || { echo "[ios-sim-selftest] SKIP: the iOS simulator exists only on macOS"; exit 0; }
mkdir -p "$MUT"

# mutant <имя> <sed-выражение> <причина, которую обязан назвать гейт>
mutant() {
  local out="$MUT/$1.mm" log
  sed -e "$2" "$SRC" >"$out"
  if cmp -s "$SRC" "$out"; then
    echo "[ios-sim-selftest] FAIL: $1 — the edit matched nothing, view.mm changed under the mutant"
    FAILS=$((FAILS + 1)); return
  fi
  log=$(bash "$ROOT/scripts/ios_sim_gate.sh" --view "$out" 2>&1)
  case "$?:$log" in
    0:*SKIP:*) echo "$log" | grep SKIP; echo "[ios-sim-selftest] SKIP: no simulator, mutants not judged"; exit 0 ;;
    0:*) echo "[ios-sim-selftest] FAIL: $1 — the gate passed a broken view"; FAILS=$((FAILS + 1)) ;;
    *"$3"*) echo "[ios-sim-selftest] ok: $1 — red for its own reason ($3)" ;;
    *) echo "$log" | grep -E "FAIL|error:" | head -5
       echo "[ios-sim-selftest] FAIL: $1 — red, but not for «$3»"; FAILS=$((FAILS + 1)) ;;
  esac
}

# Одно касание на вьюху: второй палец UIKit не доставит вовсе.
mutant single-touch 's/multipleTouchEnabled = YES/multipleTouchEnabled = NO/' \
  "only one touch reached the view"
# Все касания под одним id: маршрутизация не отличит стик от огня.
mutant collapsed-id 's/const intptr_t id = reinterpret_cast<intptr_t>(t);/const intptr_t id = 1;/' \
  "two touches share one id"

if bash "$ROOT/scripts/ios_sim_gate.sh" | tail -1 | grep -qx "\[ios-sim\] PASS"; then
  echo "[ios-sim-selftest] ok: the real view.mm passes and is back in the CMake cache"
else
  echo "[ios-sim-selftest] FAIL: the real view.mm does not pass after the mutants"; FAILS=$((FAILS + 1))
fi
echo "[ios-sim-selftest] 2 mutants + clean run, $FAILS failure(s)"
[ "$FAILS" -eq 0 ]
