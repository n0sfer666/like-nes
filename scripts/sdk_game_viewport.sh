# shellcheck shell=bash
# Настройки должны доходить до игры, собранной против префикса, а не только до unit-теста.
fixture_viewport() {
    local src dir="$WORK/fx-viewport-build" exe out
    src=$(fixture_copy viewport)
    # Этот runtime-контроль отключает доказательство покрытия: арт пилота сделан под дефолты.
    # Передачу размеров в проверку покрытия отдельно судит assetc_viewport_test на своём арте.
    sed -e 's/^zone | .*$/zone | 640 | 360/' -e 's/^zone_limit | .*$/zone_limit | 960 | 540/' \
        -e 's/ | viewport$//' "$src/game.manifest" > "$src/game.manifest.new" &&
        mv "$src/game.manifest.new" "$src/game.manifest" || return 1
    game_build "$src" "$dir" Release || {
        tail -20 "$dir.log"; sdk_bad "fixture: custom viewport did not build"; return 1; }
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --wave --frames 1 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "fixture: custom viewport did not run"; return 1; }
    grep -Fq 'neon-rumble: viewport 960x540 scale 1, visible 960x540, zone 160,90 640x360, shown 0,0 960x540, 4 strip(s)' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "fixture: custom viewport did not reach runtime"; return 1; }
    # Тик1 уже пересчитал камеру через BrawlWorld: дефолт вместо настройки дал бы 296,172.
    grep -Eq '^neon-rumble: brawl tick 1:.* camera 424,168 arena -1,' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "fixture: custom viewport did not reach simulation"; return 1; }
    sdk_ok "fixture: manifest viewport reaches the game simulation and frame against the prefix"
}
