# shellcheck shell=bash

pass=0
fail=0

check() {
    local name=$1 want=$2 needle=$3 got rc
    shift 3
    got=$("$@" 2>&1); rc=$?
    if { [ "$want" = pass ] && [ $rc = 0 ]; } || { [ "$want" = fail ] && [ $rc != 0 ]; }; then
        if [ -z "$needle" ] || printf '%s\n' "$got" | grep -qF -- "$needle"; then
            printf '%s: ok   %s\n' "${NAME:?}" "$name"
            pass=$((pass + 1))
            return
        fi
    fi
    printf '%s: FAIL %s (rc %s, want %s with "%s"):\n%s\n' "$NAME" "$name" "$rc" "$want" "$needle" "$got" >&2
    fail=$((fail + 1))
}
