# Фикстура гейта sdk-game (спека #24, В7б): уровень без объекта `bounds` — камера клампится по
# карте. Класс объекта `street` переименован, запись уровня теряет `|viewport`: с ним бейк отбил бы
# карту 640x336, которую слои при границах-карте не закрывают. Утверждение — строка сводки целиком:
# границы 0..640 x 0..336 и центр в левом нижнем углу отрезка центров (192 = 0 + 384/2,
# 228 = 336 - 216/2). Откат на нули или на прежние границы эту строку не печатает.
fixture_no_bounds() {
    local src dir="$WORK/fx-nobounds-build" exe out
    src=$(fixture_copy nobounds)
    sed 's/"type":"bounds"/"type":"street"/' "$src/levels/level1.tmj" > "$src/levels/level1.tmj.new" &&
        mv "$src/levels/level1.tmj.new" "$src/levels/level1.tmj" &&
        sed 's/ | viewport$//' "$src/game.manifest" > "$src/game.manifest.new" &&
        mv "$src/game.manifest.new" "$src/game.manifest" || {
        sdk_bad "fixture: cannot drop the bounds object in the copy"; return 1; }
    grep -q '"type":"bounds"' "$src/levels/level1.tmj" && {
        sdk_bad "fixture: the copy still has a bounds object"; return 1; }
    game_build "$src" "$dir" Release || {
        tail -20 "$dir.log"; sdk_bad "fixture: level without bounds did not build"; return 1; }
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --frames 1 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "fixture: level without bounds exited non-zero"; return 1; }
    grep -q '^neon-rumble: bounds 0..640 x 0..336, camera 192,228$' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "fixture: a level without bounds does not clamp the camera by the map"; return 1; }
    sdk_ok "fixture: a level without a bounds object clamps the camera by the map"
}
