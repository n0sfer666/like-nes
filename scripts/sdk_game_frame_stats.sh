# shellcheck shell=bash
fixture_frame_stats() {
    local dir="$WORK/frame-stats" cfg exe log
    for cfg in Release Debug; do
        log="$dir-$cfg.log"
        if ! cmake -S "$(native "$ROOT/games/neon-rumble/tests/frame-stats")" -B "$(native "$dir-$cfg")" -G "$GEN" \
            -DCMAKE_BUILD_TYPE="$cfg" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$log" 2>&1 ||
            ! cmake --build "$(native "$dir-$cfg")" >> "$log" 2>&1; then
            tail -25 "$log"; sdk_bad "frame-stats: $cfg prefix fixture build failed"; return 1
        fi
        exe="$dir-$cfg/rumble_frame_stats_fixture"
        [ ! -f "$exe.exe" ] || exe="$exe.exe"
        python3 "$ROOT/scripts/sdk_frame_stats_verdict.py" "$(native "$exe")" ||
            { sdk_bad "frame-stats: $cfg driver failed"; return 1; }
    done
    sdk_ok 'frame-stats: Release/Debug baked Ogg, real null callback, cadence and real isolated save'
}

game_frame_stats() {
    local dir=$1 cfg=$2 exe="$1/neon_rumble" log="$WORK/frame-stats-cli-$2.log" game bench rc=0
    [ ! -f "$exe.exe" ] || exe="$exe.exe"
    if [ ! -f "$dir/game.bundle" ] || [ ! -f "$dir/frame-stats.bundle" ]; then
        sdk_bad "frame-stats: $cfg missing installed game or stand bundle"; return 1
    fi
    game=$(wc -c < "$dir/game.bundle")
    bench=$(wc -c < "$dir/frame-stats.bundle")
    if [ "$bench" -le 0 ] || [ $((game + bench)) -gt 31457280 ]; then
        sdk_bad "frame-stats: $cfg empty stand bundle or assets exceed 30 MiB"; return 1
    fi
    "$exe" --frame-stats --headless --frames 1 > "$log" 2>&1 || rc=$?
    if [ "$rc" != 2 ] || ! grep -qxF 'neon-rumble: --frame-stats is a standalone window run' "$log"; then
        cat "$log"; sdk_bad "frame-stats: $cfg accepts conflicting headless arguments"; return 1
    fi
    sdk_ok "frame-stats: $cfg standalone flag and packaged game/stand assets under 30 MiB"
}
