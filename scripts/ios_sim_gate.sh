#!/usr/bin/env bash
# Гейт iOS-симулятора: оболочка собирается корнем под генератором Xcode, UI-тест
# (`platform/ios/uitest/touch_gate.m`) ставит два пальца разом — стик слева и огонь в круге справа
# внизу, — а вердикт выносит `ios_touch_verdict.py` по строкам `[touch]` из системного лога.
#
#   bash scripts/ios_sim_gate.sh                 # гейт
#   bash scripts/ios_sim_gate.sh --view <file>   # то же на подменённом view.mm (живая самопроверка)
#
# Только локально: в CI его нет по решению владельца. Без macOS, Xcode или рантайма iOS —
# пропуск ВСЛУХ с причиной и кодом 0; всё прочее, включая пропажу приватного синтеза касаний в
# новом Xcode, — провал. Симулятор устройство НЕ заменяет: GPU хостовый, жест синтезирован.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/build-ios-xcode"
VIEW="$ROOT/platform/ios/view.mm"

fail() { echo "[ios-sim] FAIL: $*" >&2; exit 1; }
ok() { echo "[ios-sim] ok: $*"; }
skip_gate() { echo "[ios-sim] SKIP: $*"; exit 0; }

case "$#:${1:-}" in
  0:) ;;
  2:--view)
    [ -f "$2" ] || fail "--view needs an existing file"
    VIEW="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")" ;;
  *) fail "usage: bash scripts/ios_sim_gate.sh [--view <view.mm>]" ;;
esac

# shellcheck source=scripts/ios_sim_lib.sh
. "$ROOT/scripts/ios_sim_lib.sh"
WHY=$(ios_sim_missing)
[ -z "$WHY" ] || skip_gate "$WHY"
RUNTIME=$(ios_runtime) || fail "simctl cannot list runtimes (CoreSimulatorService down?)"
[ -n "$RUNTIME" ] || skip_gate "no iOS simulator runtime (Xcode > Settings > Components)"

# Имя несёт рантайм: устройство навсегда остаётся на том, с которым создано, и после обновления
# Xcode гейт гонял бы старый iOS под строкой, называющей новый.
DEVICE="like-nes-gate-${RUNTIME##*.}"
UDID=$(ios_device "$DEVICE" "$RUNTIME") || fail "cannot find or create $DEVICE on $RUNTIME"
xcrun simctl bootstatus "$UDID" -b >/dev/null || fail "simulator $DEVICE ($UDID) did not boot"
ok "simulator $DEVICE $UDID on $RUNTIME"

TMP="${TMPDIR:-/tmp}"
RUN=$(mktemp -d "${TMP%/}/ios-sim-gate.XXXXXX")
# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"
FRESH="$(fresh_flag "$B" "$ROOT")"
# LIKE_NES_IOS_VIEW передаётся КАЖДЫЙ раз: кеш CMake пережил бы мутант живой самопроверки, и
# следующий обычный прогон молча судил бы подменённый view.mm.
cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$B" -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DLIKE_NES_IOS_VIEW="$VIEW" >"$RUN/configure.log" 2>&1 || fail "configure — $RUN/configure.log"

# Время начала — граница выборки лога, со смещением зоны: симулятор читает строку в своей. Строки
# прошлых прогонов отбрасывает не она, а нонс RUN_ID, который тест передаёт игре.
START=$(date '+%Y-%m-%d %H:%M:%S%z')
RUN_ID=${RUN##*.}
# Диагностику симулятора xcodebuild собирает и на зелёном прогоне, если тест оставил
# предупреждения (AVAudioSession на главном потоке), — это `simctl diagnose` на 10 минут.
if ! TEST_RUNNER_LIKE_NES_TOUCH_RUN="$RUN_ID" xcodebuild test -project "$B/like_nes.xcodeproj" -scheme like_nes_ios -configuration Debug \
    -destination "id=$UDID" -resultBundlePath "$RUN/test.xcresult" -collect-test-diagnostics never \
    >"$RUN/xcodebuild.log" 2>&1; then
  grep -E "error:|failed" "$RUN/xcodebuild.log" | tail -8 >&2 || true
  fail "xcodebuild test — $RUN/xcodebuild.log"
fi
ok "UI test passed (view: ${VIEW#"$ROOT"/})"

xcrun simctl spawn "$UDID" log show --start "$START" --style compact \
  --predicate 'process == "like_nes_ios" AND eventMessage CONTAINS "[touch]"' \
  >"$RUN/touch.log" 2>"$RUN/logshow.err" || fail "log show — $RUN/logshow.err"
python3 "$ROOT/scripts/ios_touch_verdict.py" "$RUN/touch.log" "$RUN_ID" || fail "touch routing — $RUN/touch.log"

xcrun xcresulttool export attachments --path "$RUN/test.xcresult" --output-path "$RUN/attach" \
  >/dev/null 2>&1 || fail "cannot export screenshots from $RUN/test.xcresult"
SHOTS=0
for png in "$RUN"/attach/*.png; do
  [ -f "$png" ] || continue
  SHOTS=$((SHOTS + 1))
  # Чёрный кадр — это поверхность, в которую никто не рисовал: касания доходят и до такой.
  python3 "$ROOT/scripts/png_mean.py" "$png" | awk '{ exit !($1 + $2 + $3 >= 30) }' \
    || fail "screenshot is black: $png"
done
[ "$SHOTS" -eq 2 ] || fail "expected 2 screenshots (launch, after-gesture), got $SHOTS in $RUN/attach"
ok "2 screenshots rendered, evidence in $RUN"
echo "[ios-sim] PASS"
