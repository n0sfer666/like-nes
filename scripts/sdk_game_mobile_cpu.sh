# shellcheck shell=bash
# Реальный MobileRumble на desktop SDK, без поверхности: мок только mobile_host.
mobile_cpu() {
    local dir="$WORK/mobile-cpu" exe desk="$OUT/game-Release/neon_rumble"
    {
        cmake -S "$(native "$ROOT/games/neon-rumble/tests/mobile-cpu")" -B "$(native "$dir")" -G "$GEN" \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -Dlike-nes_DIR="$(native "$PREFIX")/lib/cmake/like-nes" && cmake --build "$(native "$dir")"
    } > "$WORK/mobile-cpu-build.log" 2>&1 || {
        tail -25 "$WORK/mobile-cpu-build.log"; sdk_bad 'mobile CPU host build'; return 1; }
    exe="$dir/rumble_mobile_cpu"
    local feedback="$dir/rumble_feedback_test"
    [ ! -f "$feedback.exe" ] || feedback="$feedback.exe"
    "$feedback" || { sdk_bad 'local HitEvent haptic routing'; return 1; }
    [ ! -f "$exe.exe" ] || exe="$exe.exe"
    [ ! -f "$desk.exe" ] || desk="$desk.exe"
    local assets="$OUT/game-Release" bench="$WORK/mobile-cpu-bench.log"
    if ! "$exe" "$(native "$assets")" bench-touch > "$bench" 2>&1 ||
        ! grep -qxF 'bench-touch: PASS scripted touch joined, moved and attacked' "$bench"; then
        tail -25 "$bench"; sdk_bad 'frame-stats scripted touch through actual game'; return 1
    fi
    if ! "$exe" "$(native "$assets")" bench-conflict > "$bench" 2>&1 ||
        ! grep -qxF 'neon-rumble: --frame-stats needs the standard level and OS touch' "$bench"; then
        tail -25 "$bench"; sdk_bad 'frame-stats mobile accepts demo'; return 1
    fi
    if ! "$exe" "$(native "$assets")" network-touch > "$WORK/mobile-cpu-network.log" 2>&1 ||
        ! grep -qxF 'network-touch: PASS host tapped lobby, moved and attacked on guest' "$WORK/mobile-cpu-network.log"; then
        tail -25 "$WORK/mobile-cpu-network.log"; sdk_bad 'network lobby and combat touch'; return 1
    fi
    python3 "$ROOT/scripts/mobile_cpu_verdict.py" "$(native "$exe")" "$(native "$desk")" "$(native "$OUT/game-Release")" || {
        sdk_bad 'mobile CPU input or demo parity'; return 1; }
    python3 "$ROOT/scripts/android_touch_cpu.py" "$(native "$exe")" "$(native "$OUT/game-Release")" || {
        sdk_bad 'Android raw coordinate gestures through the real MobileRumble source'; return 1; }
    sdk_ok 'MobileRumble CPU: touch, movement, cancel, safe-only rotation, suspend; demo equals desktop'
}
