# shellcheck shell=bash

ADB_DEVICES=""
ADB_RC=0
ADB_QEMU=""
ADB_ABIS=arm64-v8a,armeabi-v7a
owner_phone_adb() {
    case $* in
        devices) printf 'List of devices attached\n%s\n' "$ADB_DEVICES"; return "$ADB_RC" ;;
        *ro.kernel.qemu) printf '%s\n' "$ADB_QEMU" ;;
        *ro.boot.qemu) printf '\n' ;;
        *abilist) printf '%s\n' "$ADB_ABIS" ;;
    esac
}
device_is() {
    local got rc
    ADB_DEVICES=$(printf '%b' "$1") ADB_QEMU=$2 ADB_ABIS=$3
    got=$(owner_phone_device); rc=$?
    printf '%s (rc %s)\n' "$got" "$rc"
    [ "$rc" = "$4" ]
}
check "no device is a skip" pass "no phone in adb devices (rc 3)" device_is "" "" "" 3
check "an emulator is a skip" pass "only an emulator" device_is 'emulator-5554\tdevice' "" "" 3
check "a qemu device is a skip" pass "only an emulator" device_is 'R5CX\tdevice' 1 "" 3
check "a network serial with a 1 is a phone" pass "10.0.0.1:41123 (rc 0)" \
    device_is '10.0.0.1:41123\tdevice' 0 arm64-v8a,armeabi-v7a 0
check "two devices fail" pass "2 devices" device_is 'R5CX\tdevice\nemulator-5554\tdevice' "" "" 1
check "an unauthorized phone does not count" pass "no phone" device_is 'R5CX\tunauthorized' "" "" 3
check "an arm64 phone is taken" pass "R5CX (rc 0)" device_is 'R5CX\tdevice' "" arm64-v8a,armeabi-v7a 0
check "an x86_64 phone fails" pass "runs x86_64,x86: §34" device_is 'R5CX\tdevice' "" x86_64,x86 1
check "arm64 under translation fails" pass "not a build under translation" \
    device_is 'R5CX\tdevice' "" x86_64,x86,arm64-v8a,armeabi-v7a 1
check "a 32-bit phone fails" pass "runs armeabi-v7a" device_is 'R5CX\tdevice' "" armeabi-v7a 1
ADB_RC=1
check "adb devices failing is a failure, not a skip" pass "adb devices failed" device_is "" "" "" 1
ADB_RC=0

ADB_BIN=stub-adb
owner_phone_host() { return 0; }
owner_phone_adb_bin() { [ -n "$ADB_BIN" ] && echo "$ADB_BIN"; }
owner_phone_iphones() { :; }
phone_stage() {
    local rc
    rm -rf "$T/p"
    ADB_DEVICES=$(printf '%b' "$1") ADB_QEMU="" ADB_ABIS=armeabi-v7a
    owner_stage_phone "$T/p"; rc=$?
    cat "$T/p/summary" "$T/p/verdict.txt"
    [ -f "$T/p/gates" ] && echo "gates file" || echo "no gates file"
    [ $rc != 0 ] || ! grep -q '^FAIL' "$T/p/verdict.txt" || return 7
    return $rc
}
check "no phone: the stage skips" pass "SKIP  phone - no phone in adb devices" phone_stage ""
check "no phone: the skip has no gates" pass "no gates file" phone_stage ""
check "two devices: the stage fails" fail "FAIL  2 devices" phone_stage 'R5CX\tdevice\nemulator-5554\tdevice'
check "two devices: no gates" fail "no gates file" phone_stage 'R5CX\tdevice\nemulator-5554\tdevice'
check "a 32-bit phone: the stage fails before the scripts" fail "FAIL  phone" phone_stage 'R5CX\tdevice'
ADB_BIN=""
check "no adb: the stage skips" pass "SKIP  phone - no adb in the Android SDK" phone_stage ""
ADB_BIN=stub-adb

cat > "$T/dc.json" <<'JSON'
{"result": {"devices": [
  {"hardwareProperties": {"reality": "physical", "platform": "iOS", "deviceType": "iPhone", "marketingName": "iPhone 15"},
   "connectionProperties": {"transportType": "wired"}, "deviceProperties": {"name": "Owner Phone"}},
  {"hardwareProperties": {"reality": "physical", "platform": "watchOS", "deviceType": "appleWatch", "marketingName": "Apple Watch"},
   "connectionProperties": {"transportType": "localNetwork"}},
  {"hardwareProperties": {"reality": "physical", "platform": "iOS", "deviceType": "iPhone", "marketingName": "iPhone 12"},
   "connectionProperties": {"tunnelState": "unavailable"}},
  {"hardwareProperties": {"reality": "simulated", "platform": "iOS", "deviceType": "iPhone", "marketingName": "iPhone 16"},
   "connectionProperties": {"transportType": "sameMachine"}}
]}}
JSON
iphones_are() {
    local got
    got=$(owner_phone_iphones_in "$T/dc.json")
    printf '%s\n' "$got"
    [ "$got" = "$1" ]
}
check "only a connected physical iPhone is named, by its model" pass "iPhone 15" iphones_are "iPhone 15"

printf '[android-rumble] ok: installed on Pixel 7, Android 15\n[android-rumble] ok: desktop headless at tick 600 hashes 0xab, as the phone\n[android-rumble] PASS\n' > "$T/r-ok"
printf '[android-rumble] FAIL: the phone hashes 0xcd at tick 600\n' > "$T/r-bad"
check "§36 passes on the phone hash" pass "tick 600 hashes 0xab, as the phone" owner_phone_judge_rumble "$T/r-ok" 0
check "§36 refuses a non-zero exit" fail "exited 1" owner_phone_judge_rumble "$T/r-ok" 1
check "§36 names the failing line" fail "the phone hashes 0xcd" owner_phone_judge_rumble "$T/r-bad" 1

bench() { printf '[bench] Pixel 7\n  frame p50 = %s us\n  frame p99 = %s us\nagainst c.txt: %s\nbrawl-resim-bench: PASS\n' "$@"; }
bench 800 1900 match > "$T/b1"
bench 820 2400 match > "$T/b2"
bench 800 1900 DIFFER > "$T/b-differ"
check "§34 passes on matching hashes" pass "p99 1900 us" owner_phone_judge_bench "$T/b1" 0 1
check "§34 refuses DIFFER" fail "against c.txt: DIFFER" owner_phone_judge_bench "$T/b-differ" 1 2
check "the worst run is the larger p99" pass "frame p99 = 2400 us" owner_phone_worst "$T/b1" "$T/b2"
check "the worst run is named" pass "worst b2:" owner_phone_worst "$T/b2" "$T/b1"
