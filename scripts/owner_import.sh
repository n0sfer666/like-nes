#!/usr/bin/env bash
set -uo pipefail

owner_import_collect() {
    local report=$1 out=$2 name from p homes
    name=$(basename "$report" .txt)
    from=$(dirname "$report")
    case $name in
        clean-check-?*) ;;
        *) echo "owner-import: $report is not a clean-check-<os>-<time>.txt report" >&2; return 1 ;;
    esac
    [ -f "$report" ] || { echo "owner-import: no $report" >&2; return 1; }
    homes=$(grep -Eo '(/Users/|/home/|[A-Za-z]:[\\/][Uu]sers[\\/])[^/\\[:space:]]+' "$report" |
        grep -Evx '(/Users/|/home/)runner|[A-Za-z]:[\\/][Uu]sers[\\/]runneradmin')
    if [ -n "$homes" ]; then
        echo "owner-import: $report names a home folder ($(printf '%s\n' "$homes" | head -1)) - it was not masked, nothing is pushed" >&2
        return 1
    fi
    rm -rf "$out"
    mkdir -p "$out"
    cp "$report" "$out/report.txt"
    for p in "$from/$name"-*.png; do
        [ ! -f "$p" ] || cp "$p" "$out/${p#"$from/$name"-}"
    done
}

owner_import_main() {
    local caller=$PWD root a f name out commit rc=0
    root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    cd "$root" || return 1
    # shellcheck source=scripts/owner_run_lib.sh
    . "$root/scripts/owner_run_lib.sh"
    [ $# -gt 0 ] || { echo "usage: bash scripts/owner_import.sh <clean-check-...txt> ..." >&2; return 2; }
    for a in "$@"; do
        case $a in
            /* | [A-Za-z]:*) f=$a ;;
            *) f=$caller/$a ;;
        esac
        name=$(basename "$f" .txt)
        out=build-owner-run/import/$name
        if owner_import_collect "$f" "$out" && commit=$(owner_run_publish "$out" "$name"); then
            echo "owner-import: pushed $commit to $OWNER_RUN_REF, folder $name"
        else
            echo "owner-import: $a is not published" >&2
            rc=1
        fi
    done
    return $rc
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    owner_import_main "$@"
fi
