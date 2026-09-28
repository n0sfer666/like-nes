#!/usr/bin/env bash
# Игра в iOS-симуляторе одной командой — шаг 2 §R `docs/owner-setup.txt`. Собирает `build-ios`
# (Ninja, те же флаги, что у `xcompile_verify.sh`), находит или заводит устройство, открывает его
# окно (Device Hub), ставит приложение и запускает его с консолью: Ctrl+C уходит в игру и закрывает
# обе.
#
#   bash scripts/ios_sim_run.sh                    # играть
#   bash scripts/ios_sim_run.sh --background-save  # уход в фон пишет achievements.save
#
# Скриптом, а не командами в runbook'е: повторный `simctl create` заводил двойника, и команды по
# имени устройства выбирали из двух; `simctl boot` уже загруженного падал; сам `boot` окна не
# показывает; а `$(…)` и перенос `\` не переживали шелл владельца (Nushell).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/build-ios"
APP=com.likenes.sidescroller

fail() { echo "[ios-run] FAIL: $*" >&2; exit 1; }
ok() { echo "[ios-run] ok: $*"; }

case "$#:${1:-}" in
  0: | 1:--background-save) ;;
  *) fail "usage: bash scripts/ios_sim_run.sh [--background-save]" ;;
esac

# shellcheck source=scripts/ios_sim_lib.sh
. "$ROOT/scripts/ios_sim_lib.sh"
WHY=$(ios_sim_missing)
[ -z "$WHY" ] || fail "$WHY"
RUNTIME=$(ios_runtime) || fail "simctl cannot list runtimes (CoreSimulatorService down?)"
[ -n "$RUNTIME" ] || fail "no iOS simulator runtime (Xcode > Settings > Components)"

echo "[ios-run] building $B (the first build compiles wgpu-native from Rust: minutes)"
# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"
FRESH="$(fresh_flag "$B" "$ROOT")"
mkdir -p "$B"
LOG="$B/run.log"
if ! { cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$B" -G Ninja -DCMAKE_SYSTEM_NAME=iOS \
         -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
       && cmake --build "$B" --target like_nes_ios; } >"$LOG" 2>&1; then
  tail -20 "$LOG" >&2
  fail "build — $LOG"
fi
ok "built build-ios/like_nes_ios.app"

# Имя несёт рантайм, как у гейта: после обновления Xcode игра встаёт на новый iOS, а не на старый.
DEVICE="like-nes-play-${RUNTIME##*.}"
UDID=$(ios_device "$DEVICE" "$RUNTIME") || fail "cannot find or create $DEVICE on $RUNTIME"
xcrun simctl bootstatus "$UDID" -b >/dev/null || fail "simulator $DEVICE ($UDID) did not boot"
# `simctl boot` окна не показывает. Окно устройства в Xcode 27 — Device Hub (`com.apple.dt.Devices`):
# Simulator.app из Xcode убран, и `open -a Simulator` отвечает «Unable to find application».
open -b com.apple.dt.Devices 2>/dev/null || open -a Simulator \
  || fail "cannot open Device Hub (Xcode > Open Developer Tool)"
xcrun simctl install "$UDID" "$B/like_nes_ios.app" || fail "cannot install the app into $DEVICE"
ok "simulator $DEVICE $UDID on $RUNTIME, app installed"
# Крестик закрывает окно Device Hub, а не приложение, и `open` окна тогда не возвращает.
echo "[ios-run] screen: Device Hub, $DEVICE in the sidebar (no window: Cmd+Tab to Device Hub)"

if [ $# -eq 0 ]; then
  exec xcrun simctl launch --console-pty --terminate-running-process "$UDID" "$APP"
fi

# Система убивает фоновый процесс без выхода, поэтому достижения обязан сохранить сам уход в фон.
DATA=$(xcrun simctl get_app_container "$UDID" "$APP" data) || fail "no data container for $APP"
SAVE="$DATA/Library/Application Support/like-nes/achievements.save"
xcrun simctl terminate "$UDID" "$APP" >/dev/null 2>&1 || true
rm -f "$SAVE"
# Подписка на уход в фон появляется вместе со строкой «iOS shell up» (`view.mm`): уйти в фон раньше —
# значит судить игру, которая ещё не слушает. Холодный первый старт бывает дольше любой паузы.
ERR="$B/background-save.stderr"
: >"$ERR"
xcrun simctl launch --stderr="$ERR" "$UDID" "$APP" >/dev/null || fail "cannot launch $APP"
for _ in $(seq 60); do
  grep -q "iOS shell up" "$ERR" && break
  sleep 1
done
grep -q "iOS shell up" "$ERR" || fail "the game did not come up in 60 s — $ERR"
# Автосейв пишет только изменившийся прогресс, а на титуле он не меняется (замер: 20 с без ухода в
# фон — файла нет), так что файл может написать только уход в фон. UIKit шлёт его с задержкой:
# замерено от 10 до 21 с после запуска Preferences.
xcrun simctl launch "$UDID" com.apple.Preferences >/dev/null || fail "cannot launch Preferences"
for _ in $(seq 60); do
  [ -f "$SAVE" ] && break
  sleep 1
done
[ -f "$SAVE" ] || fail "no achievements.save 60 s after going to background: the background save is broken"
ok "background save wrote $(stat -f %z "$SAVE") bytes to $SAVE"
echo "[ios-run] PASS"
