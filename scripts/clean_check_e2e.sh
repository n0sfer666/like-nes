# shellcheck shell=bash

case $(uname -s) in Darwin) OS=macos ;; Linux) OS=linux ;; *) OS="" ;; esac
if [ -n "$OS" ]; then
    TR=$OS-$(uname -m)
    P=$T/home/pkg
    mkdir -p "$P"
    stage "$T/st2" "$TR"
    tar -czf "$P/like-nes-engine-v9.1.0-$TR.tar.gz" -C "$T/st2" like-nes
    if [ $OS = macos ]; then
        mkdir -p "$T/vol/like-nes.app/Contents/MacOS"
        tool "$T/vol/like-nes.app/Contents/MacOS/editor_shell" "$GATE6"
        ln -s /Applications "$T/vol/Applications"
        INST=$P/like-nes-engine-v9.1.0-$TR.dmg
        hdiutil create -quiet -srcfolder "$T/vol" -volname like-nes -format UDZO "$INST"
        rm "$T/vol/Applications"
        hdiutil create -quiet -srcfolder "$T/vol" -volname like-nes -format UDZO "$T/nolink.dmg"
        check "a volume without the Applications shortcut fails" fail "lacks the Applications shortcut" \
            cc_dmg "$T/nolink.dmg" "$T" "$T/d.png"
        mkdir -p "$T/vol2"
        ln -s /Applications "$T/vol2/Applications"
        hdiutil create -quiet -srcfolder "$T/vol2" -volname like-nes -format UDZO "$T/noapp.dmg"
        check "a volume without like-nes.app fails" fail "lacks like-nes.app/Contents/MacOS/editor_shell" \
            cc_dmg "$T/noapp.dmg" "$T" "$T/d.png"
        INSTOK=".app from the .dmg: --gate6 exited 0"
        INSTPNG=-dmg.png
    else
        INST=$P/like-nes-engine-v9.1.0-$TR.AppImage
        tool "$INST" "$GATE6"
        INSTOK=".AppImage: --gate6 exited 0"
        INSTPNG=-appimage.png
    fi
    cp scripts/clean_check.sh scripts/clean_check_lib.sh "$P/"
    (cd "$P" && for f in *.tar.gz "$(basename "$INST")"; do printf '%s  %s\n' "$(cc_sha "$f")" "$f"; done > SHA256SUMS)
    mkdir -p "$T/home/tmp"
    e2e() {
        local rc
        rm -f "$P"/clean-check-*
        HOME=$T/home TMPDIR=$T/home/tmp bash "$P/clean_check.sh"; rc=$?
        cat "$P"/clean-check-*.txt
        printf '%s\n' "$P"/clean-check-*.png
        return $rc
    }
    check "the clean box run passes end to end" pass "clean-check: PASS" e2e
    check "the report masks the home folder" pass "assetc: baked ~/tmp/clean-check." e2e
    no_home() { e2e > /dev/null; ls "$P"/clean-check-*.txt > /dev/null || return 1; ! grep -h "$T/home" "$P"/clean-check-*.txt; }
    check "the report carries no home folder" pass "" no_home
    check "the run leaves its frames next to the report" pass "-tar.png" e2e
    check "the run judges the installer" pass "$INSTOK" e2e
    check "the run leaves the installer frame" pass "$INSTPNG" e2e
    check "the run sums the installer" pass "sha256 $(basename "$INST") " e2e
    readonly_dir() {
        local rc
        rm -f "$P"/clean-check-*
        chmod a-w "$P"
        HOME=$T/home TMPDIR=$T/home/tmp bash "$P/clean_check.sh"; rc=$?
        chmod u+w "$P"
        return $rc
    }
    [ "$(id -u)" = 0 ] || check "a folder it cannot write refuses" fail "cannot write" readonly_dir
    mv "$INST" "$T/inst"
    check "a missing installer fails the run" fail "no $(basename "$INST") next to the archive" e2e
    mv "$T/inst" "$INST"
    cp "$P/like-nes-engine-v9.1.0-$TR.tar.gz" "$P/like-nes-engine-v9.2.0-$TR.tar.gz"
    check "two packages refuse a guess" fail "want one like-nes-engine-<version>-$TR.tar.gz" e2e
fi
