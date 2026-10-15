#!/usr/bin/env bash
# Бенч ресима на телефоне (спека #25, В9г) — шаг 6 §R `docs/owner-setup.txt`. Берёт файл кроссплея,
# который CI написал для HEAD на macOS, кладёт его в приложение и собирает оболочку вторым входом:
# бенч на телефоне судит свои хеши тем же `--against`, что десктоп в CI.
#
#   bash scripts/mobile_bench.sh ios-sim                  # агент и владелец: прогон в симуляторе
#   bash scripts/mobile_bench.sh ios-device [--team <ID>] # проект Xcode, запуск на iPhone руками
#   bash scripts/mobile_bench.sh android                  # APK на телефон по adb, вывод из logcat
#   ... --against <файл>                                  # свой файл вместо скачанного из CI
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP=com.likenes.resimbench
VERDICT='brawl-resim-bench: (PASS|FAIL)'

fail() { echo "[mobile-bench] FAIL: $*" >&2; exit 1; }
ok() { echo "[mobile-bench] ok: $*"; }
usage() { fail "usage: bash scripts/mobile_bench.sh ios-sim|ios-device|android [--against <file>] [--team <ID>]"; }

[ $# -ge 1 ] || usage
MODE="$1"; shift
case "$MODE" in ios-sim | ios-device | android) ;; *) usage ;; esac
AGAINST="" TEAM=""
while [ $# -gt 0 ]; do
  case "$#:$1" in
    *:--against) [ $# -ge 2 ] || usage; AGAINST="$2"; shift 2 ;;
    *:--team) [ $# -ge 2 ] || usage; TEAM="$2"; shift 2 ;;
    *) usage ;;
  esac
done

if [ -z "$AGAINST" ]; then
  SHA=$(git -C "$ROOT" rev-parse HEAD)
  git -C "$ROOT" diff --quiet HEAD -- engine || echo "[mobile-bench] warning: engine/ differs from HEAD, the CI file judges HEAD"
  RUN=$(gh run list --workflow ci.yml --commit "$SHA" --status success --limit 1 --json databaseId --jq '.[0].databaseId') \
    || fail "gh cannot list CI runs (gh auth login?)"
  [ -n "$RUN" ] || fail "no successful CI run for $SHA yet: push it, then bash scripts/ci_watch.sh"
  DIR="$ROOT/build-mobile-bench/$SHA"
  mkdir -p "$DIR"
  rm -f "$DIR/crossplay-macOS.txt"
  gh run download "$RUN" --name crossplay-macOS --dir "$DIR" >/dev/null || fail "cannot download crossplay-macOS of run $RUN"
  AGAINST="$DIR/crossplay-macOS.txt"
  ok "crossplay-macOS of CI run $RUN for $SHA"
fi
[ -f "$AGAINST" ] || fail "no crossplay file: $AGAINST"
AGAINST="$(cd "$(dirname "$AGAINST")" && pwd)/$(basename "$AGAINST")"

# shellcheck source=platform/mobile/fresh_cache.sh
. "$ROOT/platform/mobile/fresh_cache.sh"

ios_configure() {
  local b="$1"; shift
  local fresh
  fresh="$(fresh_flag "$b" "$ROOT")"
  mkdir -p "$b"
  cmake ${fresh:+"$fresh"} -S "$ROOT" -B "$b" -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DLIKE_NES_MOBILE_BENCH=ON "-DLIKE_NES_BENCH_AGAINST=$AGAINST" "$@" >"$b/bench.log" 2>&1 \
    || { tail -20 "$b/bench.log" >&2; fail "configure — $b/bench.log"; }
}

judge() {
  local out="$1"
  sed -n "1,/$VERDICT/p" "$out" | sed 's/^/  /'
  grep -qE 'brawl-resim-bench: PASS' "$out" || fail "the bench on the device did not pass — $out"
  echo "[mobile-bench] PASS"
}

case "$MODE" in
ios-sim)
  # shellcheck source=scripts/ios_sim_lib.sh
  . "$ROOT/scripts/ios_sim_lib.sh"
  WHY=$(ios_sim_missing)
  [ -z "$WHY" ] || fail "$WHY"
  RUNTIME=$(ios_runtime) || fail "simctl cannot list runtimes (CoreSimulatorService down?)"
  [ -n "$RUNTIME" ] || fail "no iOS simulator runtime (Xcode > Settings > Components)"
  B="$ROOT/build-ios-bench"
  ios_configure "$B" -G Ninja -DCMAKE_OSX_SYSROOT=iphonesimulator
  cmake --build "$B" --target like_nes_resim_bench >>"$B/bench.log" 2>&1 \
    || { tail -20 "$B/bench.log" >&2; fail "build — $B/bench.log"; }
  ok "built build-ios-bench/like_nes_resim_bench.app"
  DEVICE="like-nes-play-${RUNTIME##*.}"
  UDID=$(ios_device "$DEVICE" "$RUNTIME") || fail "cannot find or create $DEVICE on $RUNTIME"
  xcrun simctl bootstatus "$UDID" -b >/dev/null || fail "simulator $DEVICE ($UDID) did not boot"
  xcrun simctl install "$UDID" "$B/like_nes_resim_bench.app" || fail "cannot install the bench into $DEVICE"
  OUT="$B/bench.stdout"
  : >"$OUT"
  xcrun simctl launch --terminate-running-process --stdout="$OUT" "$UDID" "$APP" >/dev/null \
    || fail "cannot launch $APP"
  for _ in $(seq 180); do
    grep -qE "$VERDICT" "$OUT" && break
    sleep 1
  done
  xcrun simctl terminate "$UDID" "$APP" >/dev/null 2>&1 || true
  grep -qE "$VERDICT" "$OUT" || { cat "$OUT" >&2; fail "no verdict in 180 s — $OUT"; }
  judge "$OUT"
  ;;
ios-device)
  B="$ROOT/build-ios-bench-device"
  ios_configure "$B" -G Xcode -DCMAKE_OSX_SYSROOT=iphoneos \
    ${TEAM:+"-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$TEAM"}
  ok "Xcode project build-ios-bench-device/like_nes.xcodeproj"
  echo "[mobile-bench] in Xcode: scheme like_nes_resim_bench, your iPhone as the destination, Cmd+R"
  echo "[mobile-bench] the screen stays black; the bench prints into the Xcode console and ends with brawl-resim-bench: PASS"
  open "$B/like_nes.xcodeproj"
  ;;
android)
  # shellcheck source=platform/android/android_sdk.sh
  . "$ROOT/platform/android/android_sdk.sh"
  ADB="$SDK/platform-tools/adb"
  [ -x "$ADB" ] || fail "no adb at $ADB (Android SDK Platform-Tools)"
  LOG="$ROOT/build-android-bench/bench.log"
  mkdir -p "$ROOT/build-android-bench"
  bash "$ROOT/platform/android/build_apk.sh" --bench "$AGAINST" >"$LOG" 2>&1 \
    || { tail -20 "$LOG" >&2; fail "build — $LOG"; }
  APK="$ROOT/build-android-bench/apk/like_nes_resim_bench.apk"
  ok "built build-android-bench/apk/like_nes_resim_bench.apk"
  [ "$("$ADB" devices | awk 'NR > 1 && $2 == "device"' | wc -l)" -eq 1 ] \
    || fail "need exactly one phone in adb devices (USB debugging on, the prompt on the phone accepted)"
  "$ADB" install -r "$APK" >/dev/null || fail "adb install"
  "$ADB" logcat -c
  "$ADB" shell am start -n "$APP/android.app.NativeActivity" >/dev/null || fail "adb am start"
  OUT="$ROOT/build-android-bench/bench.logcat"
  for _ in $(seq 180); do
    "$ADB" logcat -d -v raw -s like-nes >"$OUT"
    grep -qE "$VERDICT" "$OUT" && break
    sleep 1
  done
  "$ADB" shell am force-stop "$APP" || true
  grep -qE "$VERDICT" "$OUT" || { cat "$OUT" >&2; fail "no verdict in 180 s — $OUT"; }
  judge "$OUT"
  ;;
esac
