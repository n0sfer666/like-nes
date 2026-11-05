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
