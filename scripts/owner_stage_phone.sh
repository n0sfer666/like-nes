# shellcheck shell=bash

OWNER_PHONE_ADB=""
OWNER_PHONE_ABI=arm64-v8a

owner_phone_host() {
    case $(uname -s) in Darwin | Linux) return 0 ;; esac
    return 1
}

owner_phone_adb_bin() {
    local sdk
    sdk=$(. platform/android/android_sdk.sh > /dev/null 2>&1 && printf '%s' "$SDK")
    [ -n "$sdk" ] && [ -x "$sdk/platform-tools/adb" ] || return 1
    printf '%s/platform-tools/adb' "$sdk"
}

owner_phone_adb() {
    local out
    out=$("$OWNER_PHONE_ADB" "$@") || return
    printf '%s\n' "$out" | tr -d '\r'
}

owner_phone_device() {
    local all list n qemu abis
    all=$(owner_phone_adb devices) || { echo "adb devices failed - the adb server did not start"; return 1; }
    list=$(printf '%s\n' "$all" | awk 'NR > 1 && $2 == "device" { print $1 }')
    n=$(printf '%s' "$list" | grep -c .)
    [ "$n" -gt 0 ] || { echo "no phone in adb devices"; return "$OWNER_STAGE_SKIP"; }
    [ "$n" = 1 ] || { echo "$n devices in adb devices, the phone scripts take exactly one - close the emulator"; return 1; }
    qemu=$(owner_phone_adb -s "$list" shell getprop ro.kernel.qemu)$(owner_phone_adb -s "$list" shell getprop ro.boot.qemu)
    case $list in emulator-*) qemu=1 ;; esac
    case $qemu in
        *1*) echo "only an emulator ($list) in adb devices - §34 and §36 want a phone"; return "$OWNER_STAGE_SKIP" ;;
    esac
    abis=$(owner_phone_adb -s "$list" shell getprop ro.product.cpu.abilist)
    case $abis in
        "$OWNER_PHONE_ABI" | "$OWNER_PHONE_ABI",*) echo "$list" ;;
        *) echo "the phone runs $abis: §34 and §36 want $OWNER_PHONE_ABI first, not a build under translation"; return 1 ;;
    esac
}

owner_phone_judge_rumble() {
    local out=$1 rc=$2
    if [ "$rc" = 0 ] && grep -q '^\[android-rumble\] PASS' "$out" && grep -q 'as the phone$' "$out"; then
        printf 'ok    §36 %s\n' "$(grep -o 'desktop headless at tick .* as the phone$' "$out")"
        return 0
    fi
    printf 'FAIL  §36 android_rumble.sh exited %s: %s\n' "$rc" "$(grep -m1 'FAIL' "$out" || echo 'no PASS line')"
    return 1
}

owner_phone_p99() {
    awk '/frame p99 = / { print $4; exit }' "$1"
}

owner_phone_judge_bench() {
    local out=$1 rc=$2 run=$3
    if [ "$rc" = 0 ] && grep -q 'against .*: match$' "$out" && grep -q 'brawl-resim-bench: PASS' "$out"; then
        printf 'ok    §34 run %s: hashes match the CI crossplay file, p99 %s us\n' "$run" "$(owner_phone_p99 "$out")"
        return 0
    fi
    printf 'FAIL  §34 run %s, mobile_bench.sh exited %s: %s\n' "$run" "$rc" \
        "$(grep -m1 'DIFFER\|FAIL' "$out" || echo 'no match and PASS lines')"
    return 1
}

owner_phone_worst() {
    local f worst="" max=-1 p
    for f in "$@"; do
        p=$(owner_phone_p99 "$f")
        [ -n "$p" ] || continue
        if awk -v a="$p" -v b="$max" 'BEGIN { exit !(a + 0 > b + 0) }'; then max=$p worst=$f; fi
    done
    [ -n "$worst" ] || return 0
    printf 'worst %s:\n' "$(basename "$worst" .out)"
    grep -o '\[bench\].*' "$worst" | head -1 | sed 's/^/      /'
    grep 'frame p[59][09] = ' "$worst" | sed 's/^ */      /'
}

owner_phone_iphones_in() {
    python3 -I -c '
import json, sys
for d in json.load(open(sys.argv[1], encoding="utf-8"))["result"]["devices"]:
    hw, link = d.get("hardwareProperties", {}), d.get("connectionProperties", {})
    if (hw.get("reality"), hw.get("platform"), hw.get("deviceType")) != ("physical", "iOS", "iPhone"):
        continue
    if link.get("transportType") in ("wired", "localNetwork"):
        print(hw.get("marketingName") or hw.get("productType") or "an iPhone")
' "$1"
}

owner_phone_iphones() {
    local j=$1/devicectl.json
    [ "$(uname -s)" = Darwin ] || return 0
    xcrun devicectl list devices --json-output "$j" > /dev/null 2>&1 || { rm -f "$j"; return 0; }
    owner_phone_iphones_in "$j" | while IFS= read -r model; do
        printf 'note  §34 an %s is connected - the bench on it is by hand: bash scripts/mobile_bench.sh ios-device, then Cmd+R in Xcode\n' "$model"
    done
    rm -f "$j"
}

owner_phone_ci() {
    local run
    run=$(gh run list --workflow ci.yml --commit "$(git rev-parse HEAD)" --status success --limit 1 \
        --json databaseId --jq '.[0].databaseId' 2> /dev/null) || { echo "gh cannot list CI runs (gh auth login?)"; return 1; }
    [ -n "$run" ] || { echo "no successful CI run for HEAD yet - push it, bash scripts/ci_watch.sh"; return 1; }
}

owner_stage_phone() {
    local dir=$1 serial why rc=0 gates=36 i
    local logcat=build-sdk-work/game-android-$OWNER_PHONE_ABI/phone.logcat
    mkdir -p "$dir"
    owner_phone_iphones "$dir" >> "$dir/verdict.txt"
    if ! owner_phone_host; then
        owner_stage_skip "$dir" "the Android scripts run on macOS and Linux"
        owner_stage_end "$dir" phone "" $?
        return
    fi
    if ! OWNER_PHONE_ADB=$(owner_phone_adb_bin); then
        owner_stage_skip "$dir" "no adb in the Android SDK (Platform-Tools, platform/android/android_sdk.sh)"
        owner_stage_end "$dir" phone "" $?
        return
    fi
    serial=$(owner_phone_device)
    rc=$?
    if [ $rc != 0 ]; then
        if [ $rc = "$OWNER_STAGE_SKIP" ]; then
            owner_stage_skip "$dir" "$serial"
        else
            echo "FAIL  $serial" >> "$dir/verdict.txt"
        fi
        owner_stage_end "$dir" phone "" $rc
        return
    fi
    printf 'phone %s, Android %s\n' "$(owner_phone_adb -s "$serial" shell getprop ro.product.model)" \
        "$(owner_phone_adb -s "$serial" shell getprop ro.build.version.release)" >> "$dir/verdict.txt"
    echo "owner-run: phone - android_rumble.sh, then mobile_bench.sh android twice (the first build takes minutes)"
    rm -f "$logcat"
    bash scripts/android_rumble.sh --abi "$OWNER_PHONE_ABI" > "$dir/g36.out" 2>&1
    owner_phone_judge_rumble "$dir/g36.out" $? >> "$dir/verdict.txt" || rc=1
    cp "$logcat" "$dir/g36.logcat" 2> /dev/null
    if why=$(owner_phone_ci); then
        gates="34 36"
        for i in 1 2; do
            bash scripts/mobile_bench.sh android > "$dir/g34-run$i.out" 2>&1
            owner_phone_judge_bench "$dir/g34-run$i.out" $? "$i" >> "$dir/verdict.txt" || rc=1
        done
        owner_phone_worst "$dir/g34-run1.out" "$dir/g34-run2.out" >> "$dir/verdict.txt"
    else
        echo "note  §34 not run - $why" >> "$dir/verdict.txt"
    fi
    owner_stage_end "$dir" phone "$gates" $rc
}
