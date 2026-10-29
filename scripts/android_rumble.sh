#!/usr/bin/env bash
# Neon Rumble против мобильного префикса SDK на телефоне Android (спека #26, В1д; гейт §36
# docs/owner-verification.md). Ставит префикс android-<abi> из `build-sdk-android-<abi>` в пустой
# каталог, собирает игру против него в `build-sdk-work/game-android-<abi>`, ставит APK на телефон по
# adb, уводит в фон кнопкой Home и возвращает. Судит то же, что `ios_sim_rumble.sh`: возврат
# подключает поверхность к той же драке, и драка на телефоне — та же, что у десктопной игры.
#
#   bash scripts/check_sdk_game.sh --keep   # десктопный префикс: против него собирается rumble_assetc
#   bash scripts/android_rumble.sh [--abi arm64-v8a|x86_64]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOST="$ROOT/build-sdk-work/prefix"
DESK="$ROOT/build-sdk-work/game-Release"
APP=com.likenes.neonrumble

fail() { echo "[android-rumble] FAIL: $*" >&2; exit 1; }
ok() { echo "[android-rumble] ok: $*"; }
usage() { fail "usage: bash scripts/android_rumble.sh [--abi arm64-v8a|x86_64]"; }

ABI=arm64-v8a
case "$#" in 0) ;; 2) [ "$1" = --abi ] || usage; ABI=$2 ;; *) usage ;; esac
case "$ABI" in arm64-v8a | x86_64) ;; *) usage ;; esac
SDKB="$ROOT/build-sdk-android-$ABI"
GAME="$ROOT/build-sdk-work/game-android-$ABI"

[ -x "$HOST/bin/assetc" ] && [ -x "$DESK/neon_rumble" ] \
  || fail "no desktop SDK in build-sdk-work: run bash scripts/check_sdk_game.sh --keep first"
# shellcheck source=platform/android/android_sdk.sh
. "$ROOT/platform/android/android_sdk.sh"
# shellcheck source=platform/android/android_ndk.sh
. "$ROOT/platform/android/android_ndk.sh"
ADB="$SDK/platform-tools/adb"
[ -x "$ADB" ] || fail "no adb at $ADB (Android SDK Platform-Tools)"
# shellcheck source=scripts/mobile_prefix_lib.sh
. "$ROOT/scripts/mobile_prefix_lib.sh"
# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"
API=$(android_min_api) || true
[ -n "$API" ] || fail "no minSdkVersion in platform/android/AndroidManifest.xml.in"
# shellcheck source=scripts/mobile_resume_lib.sh
. "$ROOT/scripts/mobile_resume_lib.sh"
# shellcheck source=scripts/mobile_fight_lib.sh
. "$ROOT/scripts/mobile_fight_lib.sh"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$GAME"
LOG="$GAME/run.log"
: >"$LOG"
run() {
  "$@" >>"$LOG" 2>&1 || { tail -20 "$LOG" >&2; fail "$1 ${*: -1} — $LOG"; }
}

FLAGS=(-DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI="$ABI"
  -DANDROID_PLATFORM="android-$API")
echo "[android-rumble] building $SDKB (the first build compiles wgpu-native from Rust: minutes)"
FRESH=$(fresh_flag "$SDKB" "$ROOT")
run cmake ${FRESH:+"$FRESH"} -S "$ROOT" -B "$SDKB" -G Ninja -DCMAKE_BUILD_TYPE=Release "${FLAGS[@]}"
run cmake --build "$SDKB" --target like_nes_sdk
# Пустой префикс: `cmake --install` только добавляет файлы, и убранный из SDK заголовок пережил бы
# установку в старый.
PFX="$WORK/prefix"
run cmake --install "$SDKB" --component sdk --prefix "$PFX"
ok "Android SDK installed for $ABI, API $API"

# Десктопная игра пересобирается против префикса последнего `--keep`: хеш судится против этой игры,
# а не вчерашней сборки. Движок у неё — из того префикса, а Config мобильного сверяет только
# `_ln_build` (git-хеш) и незакоммиченную правку движка не видит: правил движок — `--keep` заново.
run cmake --build "$DESK"
TOOL=$(bash "$ROOT/scripts/rumble_tool.sh" "$HOST" "$ROOT/build-sdk-work/rumble-tool") || fail "rumble_assetc"
run cmake --fresh -S "$ROOT/games/neon-rumble" -B "$GAME" -G Ninja -DCMAKE_BUILD_TYPE=Release "${FLAGS[@]}" \
  -Dlike-nes_DIR="$PFX/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$HOST" -DRUMBLE_ASSETC="$TOOL"
run cmake --build "$GAME"
ok "built build-sdk-work/game-android-$ABI/neon_rumble.apk"

[ "$("$ADB" devices | awk 'NR > 1 && $2 == "device"' | wc -l)" -eq 1 ] \
  || fail "need exactly one phone in adb devices (USB debugging on, the prompt on the phone accepted)"
ABIS=$("$ADB" shell getprop ro.product.cpu.abilist | tr -d '\r')
case ",$ABIS," in
  *",$ABI,"*) ;;
  *) fail "the phone runs $ABIS, not $ABI: bash scripts/android_rumble.sh --abi <one of them>" ;;
esac
# Отказ установки печатает свою причину; две частые названы, потому что adb пишет их кодами.
"$ADB" install -r "$GAME/neon_rumble.apk" >"$WORK/install.log" 2>&1 || {
  cat "$WORK/install.log" >&2
  fail "adb install of the $ABI APK: the phone needs API $API or newer, and an install signed by another key goes first (adb uninstall $APP)"
}
"$ADB" shell am force-stop "$APP" || true
"$ADB" logcat -c
ok "installed on $("$ADB" shell getprop ro.product.model | tr -d '\r'), Android $("$ADB" shell getprop ro.build.version.release | tr -d '\r')"

OUT="$GAME/phone.logcat"
dump() { "$ADB" logcat -d -v raw -s like-nes >"$OUT"; }
has() { grep -q "$1" "$OUT"; }
detached_since() { [ -n "$(detach_after "$OUT" "$N")" ]; }
# waited <секунды> <проверка…>: снимок logcat раз в секунду, пока проверка не пройдёт.
waited() {
  local s=$1
  shift
  for _ in $(seq "$s"); do
    dump
    "$@" && return 0
    sleep 1
  done
  return 1
}
# Действие и категория лаунчера — как у иконки: интент, равный корневому интенту задачи, выводит
# живую задачу вперёд, а голый `-n` положил бы поверх неё второй экземпляр NativeActivity.
# `am start` кончается нулём и на отказе («Error: Activity class … does not exist»): судится текст.
front() {
  local said
  said=$("$ADB" shell am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER \
    -n "$APP/android.app.NativeActivity" 2>&1) || { printf '%s\n' "$said" >&2; return 1; }
  printf '%s\n' "$said" >>"$LOG"
  case "$said" in *Error*) printf '%s\n' "$said" >&2; return 1 ;; esac
}
front || fail "adb am start"
waited 60 has "surface attached #1 " || fail "the game did not come up in 60 s — $OUT"
# Несколько секунд драки до фона: тик detach должен уйти от нуля, иначе сверка с headless пуста.
sleep 5
dump
N=$(last_attach "$OUT")
"$ADB" shell input keyevent KEYCODE_HOME || fail "adb input keyevent KEYCODE_HOME"
waited 30 detached_since || fail "no surface detach after attach #$N 30 s after Home — $OUT"
front || fail "cannot bring $APP back to foreground"
waited 30 has "surface attached #$((N + 1)) " || true
resume_judged "$OUT" "$N" || fail "$RESUME_SAID"
ok "$RESUME_SAID"
desktop_same_fight "$DESK" "$RESUME_DETACHED" phone || fail "$FIGHT_SAID"
ok "$FIGHT_SAID"
echo "[android-rumble] screen: the phone shows level 1, the two fight on their own; the hand steps of §36 go on from here"
echo "[android-rumble] PASS"
