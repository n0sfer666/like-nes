#!/usr/bin/env bash
set -uo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT" || exit 1
# shellcheck source=scripts/clean_check.sh
. "$ROOT/scripts/clean_check.sh"
# shellcheck source=scripts/owner_import.sh
. "$ROOT/scripts/owner_import.sh"
NAME=clean-check-selftest
# shellcheck source=scripts/owner_selftest_lib.sh
. "$ROOT/scripts/owner_selftest_lib.sh"
T=$(mktemp -d "${TMPDIR:-/tmp}/clean-check-selftest.XXXXXX")
trap 'rm -rf "$T"' EXIT

tool() {
    printf '#!/bin/sh\n%s\n' "$2" > "$1"
    chmod +x "$1"
}
GATE6='[ "$1" = --gate6 ] || exit 9
printf '"'"'\211PNG\r\n\032\n'"'"' > "$2"'
stage() {
    local s=$1/like-nes i
    mkdir -p "$s/bin" "$s/licenses"
    tool "$s/bin/assetc" 'printf bundle > "$2"; echo "assetc: baked $2"'
    tool "$s/bin/editor_shell" "$GATE6"
    printf 'like-nes engine v9.1.0\ncommit abc123\ntarget %s\n' "$2" > "$s/version.txt"
    for i in 1 2 3 4 5 6 7; do echo "license $i" > "$s/licenses/L$i"; done
}

stage "$T/st" macos-arm64
tar -czf "$T/p.tar.gz" -C "$T/st" like-nes
printf '%s  p.tar.gz\r\n' "$(cc_sha "$T/p.tar.gz")" > "$T/SUMS"
printf '%s  p.tar.gz\n' 0000 > "$T/SUMS-bad"
check "a matching sum passes, CRLF or not" pass "ok    §15 sha256 p.tar.gz" cc_sums "$T/SUMS" "$T/p.tar.gz"
check "a wrong sum fails" fail "SHA256SUMS says 0000" cc_sums "$T/SUMS-bad" "$T/p.tar.gz"
check "a package missing from the sums fails" fail "x.dmg is not in SHA256SUMS" cc_sums "$T/SUMS" "$T/p.tar.gz" "$T/x.dmg"
check "no SHA256SUMS fails" fail "no SHA256SUMS" cc_sums "$T/none" "$T/p.tar.gz"

unpack() { rm -rf "$T/u"; cc_unpack "$1" "$T/u"; }
check "two unpacks agree" pass "no difference" unpack "$T/p.tar.gz"
tar() { command tar "$@" && case $* in *"$T/u/b"*) : > "$T/u/b/extra" ;; esac; }
check "two unpacks that differ fail" fail "two unpacks differ" unpack "$T/p.tar.gz"
unset -f tar
tar -czf "$T/flat.tar.gz" -C "$T/st/like-nes" version.txt
check "an archive without like-nes/ fails" fail "no like-nes/ folder" unpack "$T/flat.tar.gz"

V=$T/st/like-nes/version.txt
version() { printf '%b' "$1" > "$T/v.txt"; cc_version "$T/v.txt" v9.1.0 macos-arm64; }
check "version.txt of the archive passes" pass "names v9.1.0, commit abc123 and target macos-arm64" cc_version "$V" v9.1.0 macos-arm64
check "unknown fails" fail "says unknown" version 'like-nes engine v9.1.0\ncommit unknown\ntarget macos-arm64\n'
check "two lines fail" fail "has 2 lines" version 'like-nes engine v9.1.0\ntarget macos-arm64\n'
check "another triple fails" fail "does not name v9.1.0 and target macos-arm64" version 'like-nes engine v9.1.0\ncommit a\ntarget linux-x86_64\n'
check "no commit fails" fail "names no commit" version 'like-nes engine v9.1.0\nbuilt a\ntarget macos-arm64\n'

L=$T/st/like-nes/licenses
check "seven licenses pass" pass "holds 7 files, none empty" cc_licenses "$L"
mv "$L/L7" "$T/L7"
check "six licenses fail" fail "holds 6 files, not 7" cc_licenses "$L"
: > "$L/L7"
check "an empty license fails" fail "empty: L7" cc_licenses "$L"
mv "$T/L7" "$L/L7"

B=$T/st/like-nes/bin
check "assetc that bakes passes" pass "the bundle is 6 bytes" cc_assetc "$T/st/like-nes" "$T"
mkdir -p "$T/a1/bin" "$T/a2/bin"
tool "$T/a1/bin/assetc" 'echo "dyld: Library not loaded"; exit 1'
tool "$T/a2/bin/assetc" 'exit 0'
check "assetc that fails to load fails" fail "Library not loaded" cc_assetc "$T/a1" "$T/a1"
check "assetc that writes nothing fails" fail "exited 0, no bundle" cc_assetc "$T/a2" "$T/a2"

check "gate6 with a PNG passes" pass "--gate6 exited 0, g.png is 8 bytes" cc_gate6 x "$B/editor_shell" "$T/g.png" "$T/g.out"
mkdir -p "$T/cwd" "$T/logs"
tool "$T/g-ini" "$GATE6
: > imgui.ini"
in_log_dir() { (cd "$T/cwd" && cc_gate6 x "$T/g-ini" "$T/g.png" "$T/logs/g.out") && [ -f "$T/logs/imgui.ini" ] && [ ! -f "$T/cwd/imgui.ini" ] && echo "imgui.ini next to the log"; }
check "gate6 runs in the folder of its log, not next to the package" pass "imgui.ini next to the log" in_log_dir
tool "$T/g-crash" 'echo "crash in free"; exit 3'
tool "$T/g-none" 'exit 0'
tool "$T/g-text" 'echo not-a-png > "$2"'
tool "$T/g-late" "$GATE6
echo 'crash in free'; exit 3"
check "gate6 that crashes fails" fail "--gate6 exited 3, no PNG" cc_gate6 x "$T/g-crash" "$T/g.png" "$T/g.out"
check "gate6 without a PNG fails" fail "exited 0, no PNG" cc_gate6 x "$T/g-none" "$T/g.png" "$T/g.out"
check "gate6 with a non-PNG fails" fail "exited 0, no PNG" cc_gate6 x "$T/g-text" "$T/g.png" "$T/g.out"
late() { cc_gate6 x "$T/g-late" "$T/g.png" "$T/g.out" && return 0; cc_png "$T/g.png" && echo "a PNG, still refused"; return 1; }
check "gate6 that writes its PNG and crashes fails" fail "a PNG, still refused" late

tool "$T/img-ok" "$GATE6"
tool "$T/img-fuse" '[ -n "${APPIMAGE_EXTRACT_AND_RUN:-}" ] || { echo "Cannot mount AppImage, please check your FUSE setup."; exit 127; }
'"$GATE6"
tool "$T/img-nofuse" 'echo "fuse: failed to exec fusermount3: No such file or directory"; exit 127'
tool "$T/img-dead" 'echo "cannot open shared object file"; exit 127'
check "an AppImage that runs passes" pass "ok    §15 .AppImage: --gate6 exited 0" cc_appimage "$T/img-ok" "$T" "$T/i.png"
check "no FUSE falls back to extract-and-run" pass ".AppImage extracted: --gate6 exited 0" cc_appimage "$T/img-fuse" "$T" "$T/i.png"
check "the fallback that fails too fails" fail ".AppImage extracted: --gate6 exited 127" cc_appimage "$T/img-nofuse" "$T" "$T/i.png"
no_retry() {
    local out
    out=$(cc_appimage "$T/img-dead" "$T" "$T/i.png") && return 1
    printf '%s\n' "$out" | grep -q "again with" && return 1
    echo "failed once, no retry"
}
check "a missing library fails without a retry" pass "failed once, no retry" no_retry

masked() { printf '%s\n' "$2" | HOME=$1 cc_mask; }
# shellcheck disable=SC2088
check "the home folder is masked" pass "~/x and ~ and '~' and /Users/alx/y" \
    masked /Users/al/ "/Users/al/x and /Users/al and '/Users/al' and /Users/alx/y"
check "an empty home masks nothing" pass "/Users/al/x" masked "" "/Users/al/x"
check "punctuation after the home folder masks it" pass "~, ~. ~;" masked /Users/al "/Users/al, /Users/al. /Users/al;"
if command -v xattr > /dev/null 2>&1 && [ "$(uname -s)" = Darwin ]; then
    quarantined() { : > "$T/q.dmg"; xattr -w com.apple.quarantine '0081;00000000;Safari;' "$T/q.dmg"; cc_quarantine "$T/q.dmg" "$T/p.tar.gz" "$T/none"; }
    check "a quarantined package is named" pass "q.dmg carries com.apple.quarantine" quarantined
    check "a package without quarantine is not" pass "" bash -c '[ -z "$(. scripts/clean_check_lib.sh; cc_quarantine "$1")" ]' _ "$T/p.tar.gz"
fi
lic_sync() { local n; n=$(grep -c . cmake/licenses.manifest); echo "CC_LICENSES $CC_LICENSES, manifest $n"; [ "$CC_LICENSES" = "$n" ]; }
check "the license count follows cmake/licenses.manifest" pass "manifest" lic_sync
bin_sync() {
    local a b
    a=$(grep '^\$CcBin' scripts/clean_check_lib.ps1 | grep -o "'[^']*'" | tr -d "'" | LC_ALL=C sort)
    b=$(grep 'MINGW\*' scripts/release_check_lib.sh | grep -o 'like-nes/bin/[^\\]*' | sed 's#like-nes/bin/##' | LC_ALL=C sort)
    echo "ps1: $a release: $b" | tr '\n' ' '
    [ -n "$a" ] && [ "$a" = "$b" ]
}
check "the Windows bin of clean_check_lib.ps1 follows release_check_lib.sh" pass "wgpu_native.dll" bin_sync

R=$T/rep
mkdir -p "$R"
printf 'clean-check x\nok    ~/a\n' > "$R/clean-check-macos-1.txt"
: > "$R/clean-check-macos-1-tar.png"
: > "$R/clean-check-macos-1-dmg.png"
: > "$R/clean-check-macos-2-tar.png"
printf 'ok    /Users/al/a\n' > "$R/clean-check-linux-1.txt"
printf 'FAIL  x\n      /Users/runner/.cargo/a.rs\n      C:\\Users\\runneradmin\\b.rs\n      /home/runner/c\n' > "$R/clean-check-linux-2.txt"
cp "$R/clean-check-macos-1.txt" "$R/report.txt"
collected() { owner_import_collect "$R/clean-check-macos-1.txt" "$T/imp" && (cd "$T/imp" && printf '%s ' *); }
check "import collects the report and its frames" pass "dmg.png report.txt tar.png " collected
check "import refuses an unmasked report" fail "it was not masked" owner_import_collect "$R/clean-check-linux-1.txt" "$T/imp"
check "import lets the paths of CI runners through" pass "" owner_import_collect "$R/clean-check-linux-2.txt" "$T/imp2"
check "import refuses another file" fail "is not a clean-check" owner_import_collect "$R/report.txt" "$T/imp"

# shellcheck source=scripts/clean_check_e2e.sh
. "$ROOT/scripts/clean_check_e2e.sh"

printf '%s: %d ok, %d failed\n' "$NAME" "$pass" "$fail"
[ $fail = 0 ] && [ $pass -gt 0 ]
