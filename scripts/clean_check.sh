#!/usr/bin/env bash
set -uo pipefail

CC_LIB=$(dirname "${BASH_SOURCE[0]}")/clean_check_lib.sh
[ -f "$CC_LIB" ] || { echo "clean-check: copy clean_check_lib.sh next to clean_check.sh" >&2; exit 2; }
# shellcheck source=scripts/clean_check_lib.sh
. "$CC_LIB"

cc_missing() {
    echo "FAIL  §15 no $1 next to the archive - copy it from release/$2/"
    return 1
}

cc_mask() {
    local h=${HOME:-}
    while [ "${h%/}" != "$h" ]; do h=${h%/}; done
    [ -n "$h" ] || { cat; return; }
    CC_HOME=$h awk 'BEGIN { h = ENVIRON["CC_HOME"]; L = length(h) }
        {
            s = $0; out = ""
            while ((i = index(s, h)) > 0) {
                c = substr(s, i + L, 1)
                out = out substr(s, 1, i - 1) (c == "" || c !~ /[A-Za-z0-9_-]/ ? "~" : h)
                s = substr(s, i + L)
            }
            print out s
        }'
}

cc_by_hand() {
    echo "by hand - what the script cannot see:"
    case $1 in
        macos)
            echo "  1. double-click $2.dmg in Finder -> a window with like-nes.app next to an Applications shortcut"
            echo "  2. drag like-nes.app onto Applications, eject, double-click it in /Applications -> Gatekeeper: cannot verify the developer"
            echo "  3. Control-click like-nes.app > Open > Open -> the editor window opens, no Library not loaded"
            ;;
        linux)
            echo "  1. mark $2.AppImage executable in the file manager and double-click it -> the editor window opens"
            echo "  2. no like-nes entry appears in the application menu -> expected, nothing installs the .desktop"
            ;;
    esac
    echo "  send: this report and the PNG files next to it, or on the dev machine: make owner-import ARGS=<report>"
}

cc_main() {
    local os triple tgz dir stem version stamp base inst kind work
    case $(uname -s) in
        Darwin) os=macos kind=dmg ;;
        Linux) os=linux kind=AppImage ;;
        *) echo "clean-check: macOS and Linux only - on Windows: powershell -ExecutionPolicy Bypass -File clean_check.ps1" >&2; return 2 ;;
    esac
    triple=$os-$(uname -m)
    if [ $# -gt 0 ]; then
        tgz=$1
    else
        set -- "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"/like-nes-engine-*-"$triple".tar.gz
        [ $# = 1 ] && [ -f "$1" ] || { echo "clean-check: want one like-nes-engine-<version>-$triple.tar.gz next to this script" >&2; return 2; }
        tgz=$1
    fi
    [ -f "$tgz" ] || { echo "clean-check: no $tgz" >&2; return 2; }
    dir=$(cd "$(dirname "$tgz")" && pwd)
    stem=$(basename "$tgz" .tar.gz)
    tgz=$dir/$stem.tar.gz
    case $stem in
        like-nes-engine-?*-"$triple") version=${stem#like-nes-engine-}; version=${version%-"$triple"} ;;
        *) echo "clean-check: $stem.tar.gz is not a like-nes-engine-<version>-$triple package" >&2; return 2 ;;
    esac
    stamp=$(date -u '+%y%m%d-%H%M%S')
    base=$dir/clean-check-$os-$stamp
    CC_REPORT=$base.txt
    CC_RC=0
    inst=$dir/$stem.$kind
    work=$(mktemp -d "${TMPDIR:-/tmp}/clean-check.XXXXXX")
    trap 'hdiutil detach -quiet "'"$work"'/mnt" > /dev/null 2>&1; rm -rf "'"$work"'"' EXIT
    {
        echo "clean-check $stem on $os at $stamp"
        echo "system: $( { sw_vers -productVersion || sed -n 's/^PRETTY_NAME=//p' /etc/os-release; } 2> /dev/null | tr -d '"') ($(uname -srm))"
    } | cc_mask | tee "$CC_REPORT"
    [ -s "$CC_REPORT" ] || { echo "clean-check: cannot write $CC_REPORT - copy the packages to a folder you can write to" >&2; return 2; }
    [ $os = linux ] || cc_step cc_quarantine "$tgz" "$inst"
    if [ -f "$inst" ]; then
        cc_step cc_sums "$dir/SHA256SUMS" "$tgz" "$inst"
    else
        cc_step cc_sums "$dir/SHA256SUMS" "$tgz"
    fi
    cc_step cc_unpack "$tgz" "$work"
    cc_step cc_version "$work/a/like-nes/version.txt" "$version" "$triple"
    cc_step cc_licenses "$work/a/like-nes/licenses"
    cc_step cc_assetc "$work/a/like-nes" "$work"
    cc_step cc_gate6 "editor_shell from the archive" "$work/a/like-nes/bin/editor_shell" "$base-tar.png" "$work/tar.out"
    if [ ! -f "$inst" ]; then
        cc_step cc_missing "$stem.$kind" "$version"
    elif [ $os = macos ]; then
        cc_step cc_dmg "$inst" "$work" "$base-dmg.png"
    else
        cc_step cc_appimage "$inst" "$work" "$base-appimage.png"
    fi
    {
        cc_by_hand $os "$stem"
        [ $CC_RC = 0 ] && echo "clean-check: PASS" || echo "clean-check: FAIL"
    } | cc_mask | tee -a "$CC_REPORT"
    echo "clean-check: report $CC_REPORT" | cc_mask
    return $CC_RC
}

cc_step() {
    "$@" | cc_mask | tee -a "$CC_REPORT"
    [ "${PIPESTATUS[0]}" = 0 ] || CC_RC=1
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    cc_main "$@"
fi
