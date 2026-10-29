#!/usr/bin/env bash
# Neon Rumble против мобильного префикса SDK в iOS-симуляторе (спека #26, В1г). Ставит iOS-префикс
# из `build-sdk-ios-sim` в пустой каталог, собирает игру против него в `build-sdk-work/game-ios-sim`,
# запускает, уводит в фон и возвращает. Судит две вещи: возврат подключает поверхность к той же
# драке (хеш при втором attach равен хешу при detach), и драка на телефоне — та же, что у десктопной
# игры: headless той же сборки на тике detach даёт тот же хеш.
#
#   bash scripts/check_sdk_game.sh --keep   # десктопный префикс: против него собирается rumble_assetc
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
# shellcheck source=scripts/mobile_prefix_lib.sh
. "$ROOT/scripts/mobile_prefix_lib.sh"
# shellcheck source=scripts/mobile_fight_lib.sh
. "$ROOT/scripts/mobile_fight_lib.sh"
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
MIN=$(prefix_field "$PFX" _ln_ios_min) || true
[ -n "$MIN" ] || fail "no _ln_ios_min in the iOS prefix Config"
ok "iOS SDK installed, minimum iOS $MIN"

# Десктопная игра пересобирается против префикса последнего `--keep`: хеш судится против этой игры,
# а не вчерашней сборки. Движок у неё — из того префикса, а Config мобильного сверяет только
# `_ln_build` (git-хеш) и незакоммиченную правку движка не видит: правил движок — `--keep` заново.
run cmake --build "$DESK"
TOOL=$(bash "$ROOT/scripts/rumble_tool.sh" "$HOST" "$ROOT/build-sdk-work/rumble-tool") || fail "rumble_assetc"
run cmake --fresh -S "$ROOT/games/neon-rumble" -B "$GAME" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" -Dlike-nes_DIR="$PFX/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$HOST" \
  -DRUMBLE_ASSETC="$TOOL"
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
# Ждётся detach после последнего attach, а не любая: своя пара на старте (пересоздание поверхности)
# пропустила бы ожидание, и возврат пошёл бы до ухода в фон.
N=$(last_attach "$ERR")
xcrun simctl launch "$UDID" com.apple.Preferences >/dev/null || fail "cannot launch Preferences"
for _ in $(seq 60); do
  [ -n "$(detach_after "$ERR" "$N")" ] && break
  sleep 1
done
[ -n "$(detach_after "$ERR" "$N")" ] || fail "no surface detach after attach #$N 60 s after going to background — $ERR"
ios_resumed "$UDID" "$APP" "$ERR" || fail "$IOS_SAID"
ok "$IOS_SAID"

desktop_same_fight "$DESK" "$IOS_DETACHED" simulator || fail "$FIGHT_SAID"
ok "$FIGHT_SAID"
echo "[ios-rumble] screen: Device Hub, $DEVICE in the sidebar — the fight keeps running"
echo "[ios-rumble] PASS"
