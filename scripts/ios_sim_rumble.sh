#!/usr/bin/env bash
# Neon Rumble против мобильного префикса SDK в iOS-симуляторе (спека #26, В1г). Ставит iOS-префикс
# из `build-sdk-ios-sim` в пустой каталог, собирает игру против него в `build-sdk-work/game-ios-sim`,
# запускает, уводит в фон и возвращает. Судит две вещи: возврат подключает поверхность к той же
# драке (хеш при втором attach равен хешу при detach), и драка на телефоне — та же, что у десктопной
# игры: headless той же сборки на тике detach даёт тот же хеш.
#
#   bash scripts/check_sdk_game.sh --keep   # десктопный префикс: его assetc печёт бандлы игры
#   bash scripts/ios_sim_rumble.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="$ROOT/build-sdk-ios-sim"
HOST="$ROOT/build-sdk-work/prefix"
DESK="$ROOT/build-sdk-work/game-Release"
GAME="$ROOT/build-sdk-work/game-ios-sim"
APP=com.likenes.neonrumble

fail() { echo "[ios-rumble] FAIL: $*" >&2; exit 1; }
ok() { echo "[ios-rumble] ok: $*"; }

[ "$#" -eq 0 ] || fail "usage: bash scripts/ios_sim_rumble.sh"
[ -x "$HOST/bin/assetc" ] && [ -x "$DESK/neon_rumble" ] \
  || fail "no desktop SDK in build-sdk-work: run bash scripts/check_sdk_game.sh --keep first"

# shellcheck source=scripts/ios_sim_lib.sh
. "$ROOT/scripts/ios_sim_lib.sh"
WHY=$(ios_sim_missing)
[ -z "$WHY" ] || fail "$WHY"
RUNTIME=$(ios_runtime) || fail "simctl cannot list runtimes (CoreSimulatorService down?)"
[ -n "$RUNTIME" ] || fail "no iOS simulator runtime (Xcode > Settings > Components)"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$GAME"
LOG="$GAME/run.log"
: >"$LOG"
run() {
  "$@" >>"$LOG" 2>&1 || { tail -20 "$LOG" >&2; fail "$1 ${*: -1} — $LOG"; }
}

echo "[ios-rumble] building $SDK (the first build compiles wgpu-native from Rust: minutes)"
run cmake -S "$ROOT" -B "$SDK" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
run cmake --build "$SDK"
# Пустой префикс: `cmake --install` только добавляет файлы, и убранный из SDK заголовок пережил бы
# установку в старый.
PFX="$WORK/prefix"
run cmake --install "$SDK" --component sdk --prefix "$PFX"
MIN=$(sed -n 's/^set(_ln_ios_min "\(.*\)")$/\1/p' "$PFX/lib/cmake/like-nes/like-nesConfig.cmake")
ok "iOS SDK installed, minimum iOS $MIN"

# Десктопная игра пересобирается из того же дерева: хеш судится против игры, а не против вчерашней.
run cmake --build "$DESK"
run cmake --fresh -S "$ROOT/games/neon-rumble" -B "$GAME" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" -Dlike-nes_DIR="$PFX/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$HOST"
run cmake --build "$GAME"
ok "built build-sdk-work/game-ios-sim/neon_rumble.app"

DEVICE="like-nes-play-${RUNTIME##*.}"
UDID=$(ios_device "$DEVICE" "$RUNTIME") || fail "cannot find or create $DEVICE on $RUNTIME"
xcrun simctl bootstatus "$UDID" -b >/dev/null || fail "simulator $DEVICE ($UDID) did not boot"
open -b com.apple.dt.Devices 2>/dev/null || open -a Simulator \
  || fail "cannot open Device Hub (Xcode > Open Developer Tool)"
xcrun simctl terminate "$UDID" "$APP" >/dev/null 2>&1 || true
xcrun simctl install "$UDID" "$GAME/neon_rumble.app" || fail "cannot install the app into $DEVICE"
ok "simulator $DEVICE $UDID on $RUNTIME, app installed"

ERR="$GAME/background.stderr"
ios_launch_up "$UDID" "$APP" "$ERR" || fail "$IOS_SAID"
# Несколько секунд драки до фона: тик detach должен уйти от нуля, иначе сверка с headless пуста.
sleep 5
xcrun simctl launch "$UDID" com.apple.Preferences >/dev/null || fail "cannot launch Preferences"
for _ in $(seq 60); do
  grep -q "surface detached" "$ERR" && break
  sleep 1
done
grep -q "surface detached" "$ERR" || fail "no surface detach 60 s after going to background — $ERR"
ios_resumed "$UDID" "$APP" "$ERR" || fail "$IOS_SAID"
ok "$IOS_SAID"

TICK=${IOS_DETACHED#tick=}
TICK=${TICK%% *}
WORLD=${IOS_DETACHED##*world=}
[ "$TICK" -gt 0 ] || fail "detached at tick 0: the fight never ran"
HASH=$("$DESK/neon_rumble" --headless --frames "$TICK" | sed -n "s/^neon-rumble: brawl tick $TICK: hash \([0-9a-f]*\),.*/\1/p")
[ -n "$HASH" ] || fail "desktop headless printed no brawl hash at tick $TICK"
[ "$HASH" = "$WORLD" ] || fail "simulator world $WORLD at tick $TICK, desktop headless $HASH: the fight differs"
ok "desktop headless at tick $TICK hashes $HASH, as the simulator"
echo "[ios-rumble] screen: Device Hub, $DEVICE in the sidebar — the fight keeps running"
echo "[ios-rumble] PASS"
