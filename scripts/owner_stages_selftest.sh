#!/usr/bin/env bash
# Позитивный контроль стадий make owner (gpu, phone, aseprite): суждение по выводу чужого скрипта
# обязано упасть на своей сломанной фикстуре — телефона, AMD и Aseprite на раннере нет, и без
# фикстур зелёная стадия была бы неотличима от непроверенной.
#
#   bash scripts/owner_stages_selftest.sh
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
for lib in owner_run_lib owner_stage_gpu owner_stage_phone owner_stage_aseprite; do
    # shellcheck source=/dev/null
    . "$ROOT/scripts/$lib.sh"
done

NAME=owner-stages-selftest
# shellcheck source=scripts/owner_selftest_lib.sh
. "$ROOT/scripts/owner_selftest_lib.sh"
T=$(mktemp -d "${TMPDIR:-/tmp}/owner-stages.XXXXXX")
trap 'rm -rf "$T"' EXIT

g8() {
    printf '[gpu] %s | Metal | BC: yes\n' "$1"
    printf '[game] golden control: PASS (blot)\n[game] golden repeat: mean 0.0 max 0\n[game] golden repeat: PASS\n'
    printf '[game] golden frame: mean 0.1 max 3\n[game] golden frame: %s\n' "$2"
}
g8 "Apple M2" PASS > "$T/g8-ok"
g8 "Apple M2" FAIL > "$T/g8-frame"
g8 "AMD Radeon RX 6600" PASS > "$T/g8-amd"
grep -v '^\[gpu\]' "$T/g8-ok" > "$T/g8-nogpu"
check "§8 passes and keeps the gate open off AMD" pass "not AMD, §8 stays open" owner_gpu_judge_g8 "$T/g8-ok"
check "§8 refuses a frame that missed" fail 'no "golden frame: PASS"' owner_gpu_judge_g8 "$T/g8-frame"
check "§8 on AMD is the answer" pass "the AMD answer" owner_gpu_judge_g8 "$T/g8-amd"
check "§8 without [gpu] refuses" fail "no [gpu] line" owner_gpu_judge_g8 "$T/g8-nogpu"

g12() {
    printf '  lights: 5 source(s) in the table, 5 uploaded\n'
    printf '  normals: %s mapped from the table, 2 flat, 0 naming an unknown asset\n' "$1"
    printf '  occluders: 4 mapped from the table, 3 open, 0 naming an unknown asset\nlight-gpu: PASS\n'
}
g12 5 > "$T/g12-ok"
g12 4 > "$T/g12-count"
printf '  golden x: mean=0.2 max=0.9 frac=0.3\nlight-gpu: FAIL\n' > "$T/g12-pin-bad"
printf '  golden x: mean=0.0 max=0.0 frac=0.0\nlight-gpu: PASS\n' > "$T/g12-pin-ok"
printf '  golden written: x\nlight-gpu: PASS\n' > "$T/g12-pin-written"
check "§12 passes on the table counts" pass "light-gpu: PASS" owner_gpu_judge_g12 "$T/g12-ok" ""
check "§12 refuses a changed count" fail 'no "normals: 5 mapped' owner_gpu_judge_g12 "$T/g12-count" ""
check "§12 passes against the Metal reference" pass "against the Metal reference: mean=0.0" \
    owner_gpu_judge_g12 "$T/g12-ok" "$T/g12-pin-ok"
check "§12 refuses a miss of the Metal reference" fail "Metal reference: mean=0.2" \
    owner_gpu_judge_g12 "$T/g12-ok" "$T/g12-pin-bad"
check "§12 refuses a reference the run wrote instead of read" fail "golden written: x" \
    owner_gpu_judge_g12 "$T/g12-ok" "$T/g12-pin-written"

mkdir -p "$T/g0" "$T/gr/build" "$T/gr/engine/render/golden"
: > "$T/gr/build/CMakeCache.txt"
: > "$T/gr/engine/render/golden/lights_640x360.png"
: > "$T/gr/engine/render/golden/materials_640x360.png"
# shellcheck disable=SC2016
printf '#!/bin/sh\ncat "$G8_OUT"\n[ "$G8_OUT" = %s ] || : > golden_actual.png\nexit 1\n' "$T/g8-ok" > "$T/gr/build/game_sidescroller"
export G8_OUT=$T/g8-frame
printf '#!/bin/sh\ncat %s %s\n' "$T/g12-ok" "$T/g12-pin-ok" > "$T/gr/build/light_golden"
chmod +x "$T/gr/build/game_sidescroller" "$T/gr/build/light_golden"
CMAKE_RC=0
cmake() { return "$CMAKE_RC"; }
gpu_stage() {
    local rc
    rm -rf "$2/out"
    CMAKE_RC=$3
    (cd "$2" && owner_stage_gpu "$2/out"); rc=$?
    cat "$2/out/summary" "$2/out/verdict.txt"
    [ -f "$2/out/gates" ] && echo "gates: written" || echo "no gates file"
    [ ! -f "$2/out/$1" ] || echo "$1 in the stage"
    [ ! -e "$2/golden_actual.png" ] || echo "golden_actual.png in the tree"
    return $rc
}
check "gpu without build/ is a skip without gates" pass "no gates file" gpu_stage - "$T/g0" 0
check "gpu with a broken build fails without gates" fail "no gates file" gpu_stage - "$T/gr" 1
check "a missed §8 frame lands in the stage" fail "g8-actual.png in the stage" gpu_stage g8-actual.png "$T/gr" 0
check "a missed §8 frame keeps the gates" fail "gates: written" gpu_stage - "$T/gr" 0
check "the stage is judged on its §8 miss" fail 'FAIL  §8 no "golden frame: PASS"' gpu_stage - "$T/gr" 0
G8_OUT=$T/g8-ok
check "a non-zero exit fails §8 with passing lines" fail "FAIL  §8 game_sidescroller exited 1" gpu_stage - "$T/gr" 0
G8_OUT=$T/g8-frame

# shellcheck source=scripts/owner_stage_phone_cases.sh
. "$ROOT/scripts/owner_stage_phone_cases.sh"

png_head() { printf '\211PNG\r\n\032\n\000\000\000\rIHDR'; printf '%b' "$1"; }
mkdir -p "$T/ok" "$T/wide"
png_head '\000\000\002\120\000\000\001\054' > "$T/ok/queen.png"
png_head '\000\000\010\142\000\000\000\113' > "$T/wide/queen.png"
check "a 592x300 sheet passes" pass "sheet 592x300" owner_aseprite_sheet "$T/ok" cli
check "a 2146x75 sheet (one row) fails" fail "sheet 2146x75" owner_aseprite_sheet "$T/wide" cli
check "no sheet fails" fail "sheet missing" owner_aseprite_sheet "$T/none" cli
check "a relative path resolves from the caller" pass "" test "$(owner_aseprite_abs /c q.ase)" = /c/q.ase

skip_aseprite() {
    local rc
    ASEPRITE_QUEEN="" owner_stage_aseprite "$T/a" /c; rc=$?
    cat "$T/a/summary"
    [ $rc = 0 ] && [ ! -f "$T/a/gates" ]
}
check "no ASEPRITE_QUEEN is a skip without gates" pass "SKIP  aseprite - ASEPRITE_QUEEN is unset" skip_aseprite
no_aseprite() {
    local rc
    ASEPRITE_QUEEN=/nope.ase ASEPRITE=/nope owner_stage_aseprite "$T/b" /c; rc=$?
    cat "$T/b/verdict.txt" "$T/b/summary"
    [ -f "$T/b/gates" ] || echo "no gates file"
    return $rc
}
check "a missing Queen.ase fails" fail "FAIL  §19 no /nope.ase" no_aseprite
check "a missing Queen.ase writes no gates" fail "no gates file" no_aseprite
: > "$T/q.ase"
bin_is() {
    local rc
    rm -rf "$T/c"
    ASEPRITE_QUEEN=$T/q.ase ASEPRITE=$1 owner_stage_aseprite "$T/c" /c; rc=$?
    cat "$T/c/summary" "$T/c/verdict.txt"
    return $rc
}
check "a folder is not the aseprite binary" fail "no aseprite at '$T'" bin_is "$T"
check "a bare ASEPRITE resolves through PATH" fail "aseprite -b exited non-zero" bin_is env

end_is() {
    rm -rf "$T/e"; mkdir -p "$T/e"
    printf 'skip  why\n' > "$T/e/verdict.txt"
    owner_stage_end "$T/e" st "8 12" "$1"
    local rc=$?
    cat "$T/e/summary"
    [ ! -f "$T/e/gates" ] || printf 'gates file: %s\n' "$(cat "$T/e/gates")"
    return $rc
}
check "stage PASS writes summary" pass "PASS  st (gates 8 12)" end_is 0
check "stage PASS writes gates" pass "gates file: 8 12" end_is 0
check "stage FAIL returns non-zero and keeps gates" fail "gates file: 8 12" end_is 1
check "stage SKIP names the reason" pass "SKIP  st - why" end_is 3
end_bare() {
    rm -rf "$T/e"; mkdir -p "$T/e"
    : > "$T/e/verdict.txt"
    owner_stage_end "$T/e" st "" 1
    cat "$T/e/summary"
    [ ! -f "$T/e/gates" ]
}
check "stage FAIL before a judgment names no gates" pass "FAIL  st" end_bare

mkdir -p "$T/s"
printf 'ok    <b>&\n' > "$T/s/verdict.txt"
: > "$T/s/f.png"
mkdir -p "$T/s/cli"
: > "$T/s/cli/q.png"
: > "$T/s/stray.png"
owner_stage_look "$T/s" f.png 'is <it> lit'
owner_stage_look "$T/s" cli/q.png 'the sheet'
check "stage sheet escapes and captions" pass 'f: is &lt;it&gt; lit</figcaption>' owner_stage_sheet "$T/s"
check "stage sheet escapes the verdict" pass 'ok    &lt;b&gt;&amp;' owner_stage_sheet "$T/s"
check "stage sheet shows a frame from a subfolder" pass 's/cli/q.png" loading="lazy"><figcaption>cli/q: the sheet' \
    owner_stage_sheet "$T/s"
no_stray() { ! owner_stage_sheet "$T/s" | grep -q stray; }
check "stage sheet shows only the frames it was given" pass "" no_stray

printf '%s: %d ok, %d failed\n' "$NAME" "$pass" "$fail"
[ $fail = 0 ] && [ $pass -gt 0 ]
