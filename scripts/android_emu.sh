#!/usr/bin/env bash
# Android-эмулятор под `make android-rumble`: android_rumble.sh требует ровно одно устройство в
# `adb devices`, а сам эмулятор не поднимает. Устройство уже есть (телефон по USB или запущенный
# эмулятор) — не запускает второй, но ждёт конца его загрузки: adb видит `device` задолго до неё;
# иначе запускает AVD и ждёт так же.
#
#   bash scripts/android_emu.sh [<avd>]   # без имени — первый из emulator -list-avds
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

fail() { echo "[android-emu] FAIL: $*" >&2; exit 1; }
ok() { echo "[android-emu] ok: $*"; }

[ "$#" -le 1 ] || fail "usage: bash scripts/android_emu.sh [<avd>]"
# shellcheck source=platform/android/android_sdk.sh
. "$ROOT/platform/android/android_sdk.sh"
ADB="$SDK/platform-tools/adb"
EMU="$SDK/emulator/emulator"
[ -x "$ADB" ] || fail "no adb at $ADB (Android SDK Platform-Tools)"

devices() { "$ADB" devices | awk 'NR > 1 && $2 == "device"' | wc -l | tr -d ' '; }
booted() { [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ]; }

N=$(devices) || fail "adb devices failed"
[ "$N" -le 1 ] || { ok "$N devices in adb devices, no emulator started"; exit 0; }
PID=
if [ "$N" -eq 1 ]; then
  booted && { ok "a booted device is in adb devices, no emulator started"; exit 0; }
  echo "[android-emu] a device is in adb devices, waiting for it to finish booting"
else
  [ -x "$EMU" ] || fail "no emulator at $EMU (Android Studio > SDK Manager > Android Emulator)"
  AVD=${1:-$("$EMU" -list-avds | head -1)}
  [ -n "$AVD" ] || fail "no AVD (Android Studio > Device Manager > Create Virtual Device)"
  "$EMU" -list-avds | grep -qx "$AVD" || fail "no AVD named $AVD: $("$EMU" -list-avds | tr '\n' ' ')"
  LOG="${TMPDIR:-/tmp}/like-nes-android-emu.log"
  echo "[android-emu] starting $AVD (log: $LOG)"
  nohup "$EMU" -avd "$AVD" >"$LOG" 2>&1 &
  PID=$!
fi
for _ in $(seq 180); do
  [ -z "$PID" ] || kill -0 "$PID" 2>/dev/null || { tail -20 "$LOG" >&2; fail "emulator exited — $LOG"; }
  booted && break
  sleep 1
done
booted || fail "the device did not finish booting in 180 s"
ok "booted, $(devices) device(s) in adb devices"
