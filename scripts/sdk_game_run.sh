# shellcheck shell=bash
# Запуск игры гейта check_sdk_game.sh против префикса и пины её headless-вывода (спека #24, #25).
# Отдельно от sdk_game_lib.sh: там сборка и сломанные фикстуры, тут — что игра обязана напечатать.

# Бэкенд пада по ОС хоста: альтернатива трёх имён пропустила бы чужой бэкенд, слинкованный не той
# веткой CMake.
sdk_pad_backend() {
    case "$(uname -s)" in
        Darwin) echo 'GameController.framework (macOS)' ;;
        MINGW*|MSYS*|CYGWIN*) echo 'XInput (Windows)' ;;
        *) echo 'evdev (Linux)' ;;
    esac
}

# Запуск --headless --frames 480: код выхода 0 И строка вердикта. Одного кода мало — exe, не
# дошедший до цикла кадров (скажем, без бандла рядом), тоже может выйти нулём из чужой ветки.
# Сводка уровня (В5б): таблица `visual`, RGBA8 тайлсета и кадр 960x540 целиком в квадах. Ростер и
# драка (В1д, В2г спеки #25): листы, спавны, полоса, позы тика 0, попадания скрипта и хеш снапшота. С
# В4г спеки #25 rainbird на улице только после входа P2 нампадом на тике 2: поз тика 0 две, а блок
# rainbird 31..40 — улика, что ввод второго игрока дошёл до драки.
game_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --frames 480 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble exited non-zero"; return 1; }
    printf '%s\n' "$out"
    grep -q '^neon-rumble: headless run ok, 480 frames' <<< "$out" || {
        sdk_bad "$cfg: no headless verdict line"; return 1; }
    grep -q '^neon-rumble: library.bundle [1-9][0-9]* bytes' <<< "$out" || {
        sdk_bad "$cfg: library.bundle next to the exe was not loaded"; return 1; }
    grep -qxF "neon-rumble: pad backend $(sdk_pad_backend)" <<< "$out" || {
        sdk_bad "$cfg: the native pad backend of this OS is not linked from the prefix"; return 1; }
    grep -q '^neon-rumble: level level1 40x21 tile 16, 6 visual layer(s), 16 animated tile(s), 5 texture(s) 384x256 128x128 128x312 144x124 493x209$' \
        <<< "$out" || { sdk_bad "$cfg: level1 from game.bundle: no summary line"; return 1; }
    grep -q '^neon-rumble: viewport 960x540 scale 2, visible 480x270, zone 96,54 768x432, shown 0,0 960x540, 4 strip(s)$' \
        <<< "$out" || { sdk_bad "$cfg: viewport policy on 960x540 is not scale 2 with the zone centred"; return 1; }
    grep -q '^neon-rumble: viewport 300x200 scale 1, visible 300x200, zone 0,0 300x200, shown 0,0 300x200, 0 strip(s), cropped$' \
        <<< "$out" || { sdk_bad "$cfg: viewport policy on 300x200 does not crop the zone"; return 1; }
    grep -q '^neon-rumble: bounds 104..536 x 56..280, camera 296,172$' <<< "$out" || {
        sdk_bad "$cfg: camera bounds of level1 are not the street object"; return 1; }
    grep -Eq '^neon-rumble: frame 960x540 scale 2: [1-9][0-9]* sprite\(s\), [1-9][0-9]* run\(s\), 0 unknown, 0 rejected, 0 dropped$' \
        <<< "$out" || { sdk_bad "$cfg: level1 frame does not fit the quads"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (clips 51 in the table|depth band 104\.\.536 x 232\.\.264, 0 wall\(s\))$' <<< "$out")" -eq 2 ] || {
        sdk_bad "$cfg: clips table is not 3 fighters x 17 clips or the depth band is not the walk object"; return 1; }
    [ "$(grep -Ec '^neon-rumble: fighter (banderas sheet 657x642, body 200,264 facing right|rainbird sheet 602x605, body 264,240 facing left|adler sheet 548x1108, body 328,252 facing left)$' <<< "$out")" -eq 3 ] || {
        sdk_bad "$cfg: roster from game.bundle: a fighter sheet or spawn line is missing"; return 1; }
    [ "$(grep -Ec '^neon-rumble: pose tick 0: ' <<< "$out")" -eq 2 ] && [ "$(grep -Ec '^neon-rumble: pose tick 0: (adler/idle frame 0 flip 1|banderas/idle frame 0 flip 0), 0 hit, 1 hurt, 1 push, 19 overlay quad\(s\), 0 rejected, 0 dropped$' <<< "$out")" -eq 2 ] || {
        sdk_bad "$cfg: roster poses on tick 0 are not banderas and adler idle with one hurt box, one push box and an overlay"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (brawl tick 0: hash 7b9fe44a593a9b1e, banderas 200,264 y 0 hp 100, adler 328,252 y 0 hp 100, draw adler banderas|seat tick 2: P2 joining|seat tick 3: P2 present|react tick 31: rainbird block|react tick 41: rainbird stand|hit tick 53: banderas/jab -> adler, damage 6, hp 94|react tick 53: adler hurt|hit tick 66: banderas/jab -> adler, damage 6, hp 88|hit tick 75: banderas/cross -> adler, damage 8, hp 80|react tick 92: adler stand|react tick 101: banderas block|react tick 111: banderas stand|hit tick 154: banderas/jump_kick -> adler, damage 10, hp 70|react tick 154: adler fall|react tick 174: adler down|hit tick 198: banderas/jump_kick -> adler, damage 10, hp 60|react tick 209: adler getup|react tick 239: adler stand|hit tick 329: banderas/run_kick -> adler, damage 12, hp 48|react tick 329: adler fall|react tick 346: adler down|react tick 353: banderas dodge|react tick 376: adler getup|react tick 377: banderas stand|react tick 406: adler stand|hit tick 417: banderas/grab -> adler, damage 6, hp 42|react tick 417: adler thrown|react tick 436: adler down|react tick 466: adler getup|brawl tick 480: hash 35645b13b45f46a0, banderas 458,252 y 0 hp 100, rainbird 264,240 y 0 hp 100, adler 413,252 y 0 hp 42, draw rainbird banderas adler)$' <<< "$out")" -eq 30 ] && [ "$(grep -Ec '^neon-rumble: (hit|react|seat) tick ' <<< "$out")" -eq 28 ] || {
        sdk_bad "$cfg: P2 joining on tick 2, rainbird block, brawl hash, hits on adler, its hurt/fall/down/getup/thrown ticks, banderas block/dodge/grab, positions or draw order on tick 0 or after 480 scripted ticks moved"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (font monogram line 12, 390 glyph\(s\), atlas 224x156|credit (chewbatrij|monogram|puffolotti-bad-company|puffolotti-up2|warped-city) \| .*|credits screen 960x540 scale 2: 5 pack\(s\), 2 page\(s\), [1-9][0-9]* line\(s\), [1-9][0-9]* glyph\(s\), 0 unknown, [1-9][0-9]* quad\(s\), 0 dropped)$' <<< "$out")" -eq 7 ] || {
        sdk_bad "$cfg: font monogram, five packs of credits.txt or a two-page credits screen with 0 unknown and 0 dropped missing"; return 1; }
    sdk_ok "$cfg: built against the prefix and ran 480 headless frames"
}
