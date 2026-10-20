# shellcheck shell=bash

CC_LICENSES=7

cc_sha() {
    if command -v sha256sum > /dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | awk '{ print $1 }'
}

cc_sums() {
    local sums=$1 f name want got rc=0
    shift
    if [ ! -f "$sums" ]; then
        echo "FAIL  §15 no SHA256SUMS next to the package - copy it from release/<version>/"
        return 1
    fi
    for f in "$@"; do
        name=$(basename "$f")
        want=$(awk -v f="$name" '{ sub(/\r$/, "") } $2 == f || $2 == "*" f { print $1 }' "$sums")
        got=$(cc_sha "$f")
        if [ -z "$want" ]; then
            echo "FAIL  §15 $name is not in SHA256SUMS"; rc=1
        elif [ "$want" = "$got" ]; then
            echo "ok    §15 sha256 $name $got"
        else
            echo "FAIL  §15 sha256 $name is $got, SHA256SUMS says $want"; rc=1
        fi
    done
    return $rc
}

cc_unpack() {
    local tgz=$1 work=$2 d
    for d in a b; do
        mkdir -p "$work/$d"
        tar -xzf "$tgz" -C "$work/$d" > "$work/tar-$d.out" 2>&1 \
            || { echo "FAIL  §15 tar -xzf into $d: $(tail -1 "$work/tar-$d.out")"; return 1; }
    done
    if ! diff -r "$work/a" "$work/b" > "$work/unpack.diff" 2>&1; then
        echo "FAIL  §15 two unpacks differ: $(head -1 "$work/unpack.diff")"
        return 1
    fi
    [ -d "$work/a/like-nes" ] || { echo "FAIL  §15 the archive holds no like-nes/ folder"; return 1; }
    echo "ok    §15 unpacked twice, diff -r finds no difference"
}

cc_version() {
    local file=$1 version=$2 triple=$3 n
    [ -f "$file" ] || { echo "FAIL  §15 no like-nes/version.txt"; return 1; }
    sed 's/^/      /' "$file"
    n=$(grep -c '' "$file")
    if [ "$n" != 3 ]; then
        echo "FAIL  §15 version.txt has $n lines, not 3"; return 1
    elif grep -qi unknown "$file"; then
        echo "FAIL  §15 version.txt says unknown"; return 1
    elif [ "$(sed -n 1p "$file")" != "like-nes engine $version" ] || [ "$(sed -n 3p "$file")" != "target $triple" ]; then
        echo "FAIL  §15 version.txt does not name $version and target $triple of the archive name"; return 1
    fi
    case $(sed -n 2p "$file") in
        "commit "?*) echo "ok    §15 version.txt names $version, $(sed -n 2p "$file") and target $triple" ;;
        *) echo "FAIL  §15 the second line of version.txt names no commit"; return 1 ;;
    esac
}

cc_licenses() {
    local n empty
    n=$(find "$1" -type f 2> /dev/null | grep -c .)
    empty=$(find "$1" -type f -size 0 2> /dev/null | sed 's|.*/||' | tr '\n' ' ')
    if [ "$n" = "$CC_LICENSES" ] && [ -z "$empty" ]; then
        echo "ok    §15 licenses/ holds $n files, none empty"
        return 0
    fi
    echo "FAIL  §15 licenses/ holds $n files, not $CC_LICENSES${empty:+ - empty: $empty}"
    return 1
}

cc_assetc() {
    local tree=$1 bundle=$2/probe.bundle out rc
    out=$("$tree/bin/assetc" --synthetic "$bundle" 2>&1)
    rc=$?
    printf '%s\n' "$out" | head -3 | sed 's/^/      /'
    if [ $rc = 0 ] && [ -s "$bundle" ]; then
        echo "ok    §15 assetc --synthetic exited 0, the bundle is $(wc -c < "$bundle" | tr -d ' ') bytes"
        return 0
    fi
    echo "FAIL  §15 assetc --synthetic exited $rc$([ -s "$bundle" ] || echo ', no bundle')"
    return 1
}

cc_png() {
    [ "$(od -An -tx1 -N8 "$1" 2> /dev/null | tr -d ' \n')" = 89504e470d0a1a0a ]
}

cc_gate6() {
    local what=$1 exe=$2 png=$3 log=$4 rc
    rm -f "$png"
    (cd "$(dirname "$log")" && "$exe" --gate6 "$png") > "$log" 2>&1
    rc=$?
    if [ $rc = 0 ] && cc_png "$png"; then
        echo "ok    §15 $what: --gate6 exited 0, $(basename "$png") is $(wc -c < "$png" | tr -d ' ') bytes"
        return 0
    fi
    tail -5 "$log" | sed 's/^/      /'
    echo "FAIL  §15 $what: --gate6 exited $rc$(cc_png "$png" || echo ', no PNG')"
    return 1
}

cc_dmg() {
    local dmg=$1 work=$2 png=$3 mnt=$2/mnt rc=0 exe
    mkdir -p "$mnt"
    if ! hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$mnt" "$dmg" > "$work/attach.out" 2>&1; then
        echo "FAIL  §15 hdiutil attach $(basename "$dmg"): $(tail -1 "$work/attach.out")"
        return 1
    fi
    exe=$mnt/like-nes.app/Contents/MacOS/editor_shell
    if [ -x "$exe" ] && [ -L "$mnt/Applications" ]; then
        echo "ok    §15 the volume holds like-nes.app and the Applications shortcut"
    else
        echo "FAIL  §15 the volume lacks $([ -x "$exe" ] && echo 'the Applications shortcut' || echo 'like-nes.app/Contents/MacOS/editor_shell')"
        rc=1
    fi
    [ ! -x "$exe" ] || cc_gate6 ".app from the .dmg" "$exe" "$png" "$work/dmg.out" || rc=1
    hdiutil detach -quiet "$mnt" || { echo "FAIL  §15 hdiutil detach left $mnt mounted"; rc=1; }
    return $rc
}

cc_appimage() {
    local img=$1 work=$2 png=$3 first
    chmod +x "$img" 2> /dev/null
    first=$(cc_gate6 .AppImage "$img" "$png" "$work/appimage.out") && { printf '%s\n' "$first"; return 0; }
    if ! grep -Eq 'libfuse|FUSE|fusermount|/dev/fuse' "$work/appimage.out"; then
        printf '%s\n' "$first"
        return 1
    fi
    echo "note  §15 no FUSE on this box - again with APPIMAGE_EXTRACT_AND_RUN=1, the documented fallback"
    APPIMAGE_EXTRACT_AND_RUN=1 cc_gate6 ".AppImage extracted" "$img" "$png" "$work/appimage.out"
}

cc_quarantine() {
    local f
    for f in "$@"; do
        xattr -p com.apple.quarantine "$f" > /dev/null 2>&1 || continue
        echo "note  §15 $(basename "$f") carries com.apple.quarantine - a refusal below may be Gatekeeper, not the package"
    done
    return 0
}
