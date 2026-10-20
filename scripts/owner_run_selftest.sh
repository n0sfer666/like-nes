#!/usr/bin/env bash
# Позитивный контроль owner_run_lib.sh: каждое суждение прогона владельца обязано упасть на своей
# сломанной фикстуре и пройти на исправной, иначе зелёный owner-run неотличим от непроверенного.
#
#   bash scripts/owner_run_selftest.sh
#
# Случаи с бинарём игры идут, когда build-sdk-work/game-Release/neon_rumble собран; без него они
# названы пропущенными, а не засчитаны.
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/owner_run_lib.sh
. "$ROOT/scripts/owner_run_lib.sh"
# shellcheck source=scripts/owner_gates_lib.sh
. "$ROOT/scripts/owner_gates_lib.sh"

NAME=owner-run-selftest
pass=0; fail=0
T=$(mktemp -d "${TMPDIR:-/tmp}/owner-run.XXXXXX")
trap 'rm -rf "$T"' EXIT

check() {
    local name=$1 want=$2 needle=$3 got rc
    shift 3
    got=$("$@" 2>&1); rc=$?
    if { [ "$want" = pass ] && [ $rc = 0 ]; } || { [ "$want" = fail ] && [ $rc != 0 ]; }; then
        if [ -z "$needle" ] || printf '%s\n' "$got" | grep -qF -- "$needle"; then
            printf '%s: ok   %s\n' "$NAME" "$name"
            pass=$((pass + 1))
            return
        fi
    fi
    printf '%s: FAIL %s (rc %s, want %s with "%s"):\n%s\n' "$NAME" "$name" "$rc" "$want" "$needle" "$got" >&2
    fail=$((fail + 1))
}

hit() { printf 'neon-rumble: hit tick %s banderas jab -> adler\n' "$1"; }
react() { printf 'neon-rumble: react tick %s adler stagger\n' "$1"; }

printf '! expect hit tick 45 \r\n! expect react tick 46\r\n! absent hit tick 9[0-9] \r\n' > "$T/crlf.keys"
{ hit 45; react 46; } > "$T/ordered.out"
{ react 46; hit 45; } > "$T/reversed.out"
{ hit 45; react 46; hit 95; } > "$T/absent.out"

check "directive drops CR of a CRLF scenario" pass "" \
    test "$(owner_run_directive "$T/crlf.keys" expect | tail -1)" = "react tick 46"
check "expect in order passes" pass "ok    expect react tick 46" owner_run_expect "$T/crlf.keys" "$T/ordered.out"
check "expect out of order fails at the first missing line" fail "FAIL  expect react tick 46 (and 0 after it)" \
    owner_run_expect "$T/crlf.keys" "$T/reversed.out"
check "absent not met passes" pass "ok    absent hit tick 9[0-9]" owner_run_absent "$T/crlf.keys" "$T/ordered.out"
check "absent met fails with the line" fail "hit tick 95 banderas" owner_run_absent "$T/crlf.keys" "$T/absent.out"
printf '%s\r\n' '! expect hit tick 9000' 'menu 2 tap ENTER' 'level' '# 8000 down D' '75 tap ESCAPE' 'menu 200 tap ENTER' \
    'level' '1 down D' '30 quit' > "$T/span.keys"
span_is() { [ "$(owner_run_span "$1")" = "$2" ]; }
check "window cap spans the last menu frame and the last tick of every level track" pass "" \
    span_is "$T/span.keys" 305
check "window cap is the exact span, not a longer number" fail "" span_is "$T/span.keys" 3050
check "no directives judge nothing" pass "" owner_run_judge /dev/null "$T/ordered.out" ""

{ echo 'neon-rumble: join seat 1'; echo 'neon-rumble: brawl tick 0 hash 0'; hit 45; react 46; } > "$T/headless.out"
{ echo 'shot jab frame 50 tick 45'; hit 45; react 46; echo 'neon-rumble: window run ok, 480 frames'; } > "$T/window.out"
{ hit 45; react 47; } > "$T/drift.out"
: > "$T/empty.out"
check "same as headless ignores headless-only and window-only lines" pass "same as headless: 2 line(s)" \
    owner_run_same "$T/window.out" "$T/headless.out"
check "differs from headless fails with the diff" fail "> neon-rumble: react tick 47" \
    owner_run_same "$T/drift.out" "$T/headless.out"
check "empty headless fails, not passes" fail "no lines to compare" owner_run_same "$T/empty.out" "$T/empty.out"
check "judge fails when only the headless half fails" fail "FAIL  differs" \
    owner_run_judge "$T/crlf.keys" "$T/drift.out" "$T/headless.out"
{ hit 45; react 46; } | sed 's/$/\r/' > "$T/window-crlf.out"
strip_then_same() {
    [ "$(tr -cd '\r' < "$1" | wc -c)" -gt 0 ] || { echo "the fixture has no CR"; return 1; }
    owner_run_strip_cr "$1" && owner_run_same "$1" "$2"
}
check "CRLF window output matches headless after strip" pass "same as headless: 2 line(s)" \
    strip_then_same "$T/window-crlf.out" "$T/headless.out"

printf 'menu 1 shot title\r\nlevel\n45 shot jab\n# 50 shot gone\n#60 shot glued\n' > "$T/shots.keys"
{ echo 'shot title frame 1 tick 0'; echo 'shot jab frame 50 tick 45'; } > "$T/shots-all.out"
echo 'shot title frame 1 tick 0' > "$T/shots-short.out"
check "every frame of the scenario written passes" pass "ok    frames: 2" \
    owner_run_shots "$T/shots.keys" "$T/shots-all.out"
check "a frame the window did not write fails by name" fail "frames not written: jab" \
    owner_run_shots "$T/shots.keys" "$T/shots-short.out"

publish_case() {
    local w=$T/pub
    rm -rf "$w" "$T/origin.git" && mkdir -p "$w/run1/a" "$w/run2" || return 1
    export GIT_CONFIG_GLOBAL=$T/gitconfig GIT_CONFIG_NOSYSTEM=1
    printf '[user]\n\tname = Owner\n\temail = owner@example.invalid\n' > "$GIT_CONFIG_GLOBAL"
    git init -q --bare "$T/origin.git" && git init -q "$w/repo" || return 1
    (cd "$w/repo" && echo x > f && git add f && git commit -qm init && git remote add origin "$T/origin.git") || return 1
    echo one > "$w/run1/a/x.txt"; echo two > "$w/run2/y.txt"
    (cd "$w/repo" && echo dirty > f && echo staged > g && git add g && git status --porcelain) > "$w/before"
    (cd "$w/repo" && owner_run_publish "$w/run1" r1 > /dev/null && owner_run_publish "$w/run2/" r2 > /dev/null) \
        || { echo "publish failed"; return 1; }
    (cd "$w/repo" && git status --porcelain) | cmp -s - "$w/before" || { echo "owner index or tree changed"; return 1; }
    git -C "$T/origin.git" rev-list --parents "$OWNER_RUN_REF" | awk '{ print NF }' | tr '\n' ' ' \
        | grep -qx '2 1 ' || { echo "want a root commit and one child"; return 1; }
    git -C "$T/origin.git" ls-tree -r --name-only "$OWNER_RUN_REF" | tr '\n' ' ' \
        | grep -qx 'r1/a/x.txt r2/y.txt ' || { echo "the second report dropped the first"; return 1; }
    git -C "$T/origin.git" log -1 --format=%B "$OWNER_RUN_REF" | grep -qx 'Signed-off-by: Owner <owner@example.invalid>' \
        || { echo "no sign-off"; return 1; }
    [ ! -e "$w/run1.index" ] && [ ! -e "$w/run2.files" ] || { echo "temp index left behind"; return 1; }
    (cd "$w/repo" && git remote set-url origin "$T/nope.git" && owner_run_publish "$w/run1" r3) && return 1
    echo "refused push returns non-zero"
}
check "publish keeps history, the owner's index and signs off" pass "refused push returns non-zero" publish_case
check "publish without a report folder refuses" fail "no report folder" owner_run_publish "$T/none" r0

DOC=docs/owner-verification.md
first=$(owner_gates_open_lines "$DOC" | head -1)
sect=${first##*§}; sect=${sect%% *}
printf '! gate %s\n' "$sect" > "$T/cover.keys"
printf '! gate %s0\n' "$sect" > "$T/prefix.keys"
manual_keeps() {
    local got
    got=$(owner_run_manual "$DOC" "$T/cover.keys")
    printf '%s\n' "$got"
    printf '%s\n' "$got" | grep -qF "(§$sect руководства)"
}
no_sheet_line() { ! owner_run_manual "$DOC" "$@" | grep -q 'scenarios drive'; }
check "no scenario - no sheet line" pass "" no_sheet_line /dev/null
check "covered gate §$sect is named on the sheet line" pass "scenarios drive §$sect -" manual_keeps
check "a gate number is not matched as a prefix" pass "" no_sheet_line "$T/prefix.keys"

{ printf '<b>&\n'; } | owner_run_html_escape > "$T/esc.out"
check "html escape covers & < >" pass "" test "$(cat "$T/esc.out")" = "&lt;b&gt;&amp;"

EXE=$ROOT/build-sdk-work/game-Release/neon_rumble
[ -f "$EXE.exe" ] && EXE=$EXE.exe
if [ -x "$EXE" ]; then
    printf 'menu 1 tap NOSUCHKEY\n' > "$T/badkey.keys"
    printf 'menu 1 jump ENTER\n' > "$T/badverb.keys"
    printf 'menu 1 tap ENTER\nlevel\n10 quit\n' > "$T/good.keys"
    exit_is() { local want=$1; shift; "$@" > /dev/null 2>&1; [ $? = "$want" ]; }
    check "unknown key in a scenario refuses the run" pass "" exit_is 2 "$EXE" --frames 30 --script "$T/badkey.keys"
    check "unknown verb in a scenario refuses the run" pass "" exit_is 2 "$EXE" --frames 30 --script "$T/badverb.keys"
    check "--script with --headless refuses the run" pass "" exit_is 2 "$EXE" --frames 30 --headless --script "$T/good.keys"
    check "--shots without --script refuses the run" pass "" exit_is 2 "$EXE" --frames 30 --shots "$T"
    bad_script() { printf "$1" > "$T/bad.keys"; "$EXE" --frames 30 --script "$T/bad.keys" 2>&1; [ $? = 2 ]; }
    check "a scenario without quit refuses the run" pass "no 'quit' step" bad_script 'menu 1 tap ENTER\n'
    check "an extra word refuses the run" pass "extra word 'J'" bad_script 'menu 1 tap ENTER J\nmenu 9 quit\n'
    check "shot without a name refuses the run" pass "'shot' needs a frame name" bad_script 'menu 1 shot\nmenu 9 quit\n'
    check "a frame name outside [A-Za-z0-9_-] refuses the run" pass "is not [A-Za-z0-9_-]+" \
        bad_script 'menu 1 shot ../x\nmenu 9 quit\n'
    check "a frame name used twice refuses the run" pass "used twice" bad_script 'menu 1 shot a\nmenu 2 shot a\nmenu 9 quit\n'
    check "frame names differing only in case refuse the run" pass "used twice" \
        bad_script 'menu 1 shot Jab\nmenu 2 shot jab\nmenu 9 quit\n'
    check "a step without a verb names the verb" pass "unknown verb ''" bad_script 'menu 1\nmenu 9 quit\n'
    check "a tick past the bound refuses the run" pass "bad time '4294967295'" bad_script 'menu 4294967295 tap A\nmenu 9 quit\n'
else
    printf '%s: skip binary cases - %s is not built\n' "$NAME" "$EXE"
fi

printf '%s: %d ok, %d failed\n' "$NAME" "$pass" "$fail"
[ $fail = 0 ] && [ $pass -gt 0 ]
