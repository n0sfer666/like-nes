#!/usr/bin/env bash
# Оконная половина гейтов Neon Rumble без рук: сборка против SDK, окно по каждому сценарию из
# games/neon-rumble/owner/*.keys (клавиши подаёт сценарий, живые пады и клавиатура отключены),
# машинная сверка строк hit/react, лист кадров index.html и отчёт коммитом в ref refs/owner-runs/main.
#
#   bash scripts/owner_run.sh [--no-build] [--no-push] [сценарий.keys ...]
#
# Глазами остаётся то, что машина не судит: лист кадров и гейты без сценария — их список в конце.
set -uo pipefail

CALLER=$PWD
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/owner_run_lib.sh
. "$ROOT/scripts/owner_run_lib.sh"
# shellcheck source=scripts/owner_gates_lib.sh
. "$ROOT/scripts/owner_gates_lib.sh"
# shellcheck source=scripts/redact_home_lib.sh
. "$ROOT/scripts/redact_home_lib.sh"

BUILD=1
PUSH=1
SCENARIOS=()
for a in "$@"; do
    case $a in
        --no-build) BUILD=0 ;;
        --no-push) PUSH=0 ;;
        -*) echo "usage: bash scripts/owner_run.sh [--no-build] [--no-push] [scenario.keys ...]" >&2; exit 2 ;;
        /* | [A-Za-z]:*) SCENARIOS+=("$a") ;;
        *) SCENARIOS+=("$CALLER/$a") ;;
    esac
done
[ ${#SCENARIOS[@]} -gt 0 ] || SCENARIOS=("$ROOT"/games/neon-rumble/owner/*.keys)
dup=$(for k in "${SCENARIOS[@]}"; do basename "$k" .keys; done | sort | uniq -d)
[ -z "$dup" ] || { echo "owner-run: two scenarios share the name $dup, their reports would collide" >&2; exit 2; }

case $(uname -s) in
    Darwin) OS_TAG=macos ;;
    Linux) OS_TAG=linux-${XDG_SESSION_TYPE:-tty} ;;
    MINGW* | MSYS* | CYGWIN*) OS_TAG=windows ;;
    *) OS_TAG=$(uname -s | tr '[:upper:]' '[:lower:]') ;;
esac
RUN=$(date -u '+%y%m%d-%H%M%S')-$OS_TAG
OUT=build-owner-run/$RUN
OWNER_RUN_FRAMES=3600
OWNER_RUN_SLACK=1200
mkdir -p "$OUT"
EXE=$ROOT/build-sdk-work/game-Release/neon_rumble
[ -f "$EXE.exe" ] && EXE=$EXE.exe

echo "owner-run: $RUN at $(git rev-parse --short HEAD), report in $OUT"
if [ $BUILD = 1 ]; then
    echo "owner-run: build - bash scripts/check_sdk_game.sh --keep, log in $OUT/build.log"
    if ! bash scripts/check_sdk_game.sh --keep > "$OUT/build.log" 2>&1; then
        tail -20 "$OUT/build.log"
        echo "owner-run: FAIL - the build did not pass, see $OUT/build.log" >&2
        exit 1
    fi
fi
[ -x "$EXE" ] || { echo "owner-run: FAIL - $EXE is missing, run without --no-build" >&2; exit 1; }

fail=0
SUMMARY=$OUT/summary.txt
: > "$SUMMARY"
for keys in "${SCENARIOS[@]}"; do
    name=$(basename "$keys" .keys)
    frames=$(owner_run_directive "$keys" same-as-headless)
    case $frames in
        *[!0-9]*)
            printf 'FAIL  %s - "! same-as-headless %s" is not one number\n' "$name" "$frames" | tee -a "$SUMMARY"
            fail=1
            continue ;;
    esac
    dir=$OUT/$name
    mkdir -p "$dir"
    cp "$keys" "$dir/$name.keys"
    read -r -a args <<< "$(owner_run_directive "$keys" args)"
    cap=$((${frames:-$OWNER_RUN_FRAMES} + OWNER_RUN_SLACK))
    (cd "$dir" && "$EXE" ${args[@]+"${args[@]}"} --frames "$cap" --script "$name.keys" --shots . \
        > window.out 2> window.err)
    rc=$?
    headless=""
    hrc=0
    if [ -n "$frames" ]; then
        (cd "$dir" && "$EXE" ${args[@]+"${args[@]}"} --headless --frames "$frames" > headless.out 2> headless.err)
        hrc=$?
        headless=$dir/headless.out
    fi
    owner_run_strip_cr "$dir/window.out" "$dir/headless.out"
    owner_run_judge "$keys" "$dir/window.out" "$headless" > "$dir/verdict.txt"
    jrc=$?
    [ $rc = 0 ] || printf 'FAIL  window exit code %s (cap %s frames), stderr in window.err\n' "$rc" "$cap" >> "$dir/verdict.txt"
    [ $hrc = 0 ] || printf 'FAIL  headless exit code %s, stderr in headless.err\n' "$hrc" >> "$dir/verdict.txt"
    verdict=PASS
    [ $rc = 0 ] && [ $hrc = 0 ] && [ $jrc = 0 ] || { verdict=FAIL; fail=1; }
    printf '%-5s %s (gates %s)\n' "$verdict" "$name" "$(owner_run_directive "$keys" gate)" | tee -a "$SUMMARY"
    grep '^FAIL' "$dir/verdict.txt" | sed 's/^/      /'
done

masked=1
for f in "$OUT"/*.txt "$OUT"/*.log "$OUT"/*/*.out "$OUT"/*/*.err "$OUT"/*/*.txt; do redact_home_file "$f" || masked=0; done
owner_run_sheet "$OUT" "$RUN" > "$OUT/index.html"
redact_home_file "$OUT/index.html" || masked=0
echo "owner-run: frame sheet $OUT/index.html"

if [ $masked = 0 ]; then
    echo "owner-run: FAIL - the home folder is not masked in $OUT, nothing is pushed" >&2
    fail=1
elif [ $PUSH = 1 ]; then
    if commit=$(owner_run_publish "$OUT" "$RUN"); then
        echo "owner-run: pushed $commit to $OWNER_RUN_REF, folder $RUN"
    else
        echo "owner-run: push to $OWNER_RUN_REF failed - send $OUT by hand" >&2
        fail=1
    fi
fi

echo "owner-run: open gates - what is left by hand:"
owner_run_manual docs/owner-verification.md "${SCENARIOS[@]}"
[ $fail = 0 ] && echo "owner-run: PASS" || echo "owner-run: FAIL"
exit $fail
