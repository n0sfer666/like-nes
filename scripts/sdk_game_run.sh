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
    grep -q '^neon-rumble: level level1 104x21 tile 16, 6 visual layer(s), 16 animated tile(s), 5 texture(s) 384x256 128x128 128x312 144x124 493x209$' \
        <<< "$out" || { sdk_bad "$cfg: level1 from game.bundle: no summary line"; return 1; }
    grep -q '^neon-rumble: viewport 960x540 scale 2, visible 480x270, zone 96,54 768x432, shown 0,0 960x540, 4 strip(s)$' \
        <<< "$out" || { sdk_bad "$cfg: viewport policy on 960x540 is not scale 2 with the zone centred"; return 1; }
    grep -q '^neon-rumble: viewport 300x200 scale 1, visible 300x200, zone 0,0 300x200, shown 0,0 300x200, 0 strip(s), cropped$' \
        <<< "$out" || { sdk_bad "$cfg: viewport policy on 300x200 does not crop the zone"; return 1; }
    grep -q '^neon-rumble: bounds 104..1560 x 56..280, camera 296,172$' <<< "$out" || {
        sdk_bad "$cfg: camera bounds of level1 are not the street object"; return 1; }
    grep -Eq '^neon-rumble: frame 960x540 scale 2: [1-9][0-9]* sprite\(s\), [1-9][0-9]* run\(s\), 0 unknown, 0 rejected, 0 dropped$' \
        <<< "$out" || { sdk_bad "$cfg: level1 frame does not fit the quads"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (clips 51 in the table|depth band 104\.\.1560 x 232\.\.264, 0 wall\(s\))$' <<< "$out")" -eq 2 ] || {
        sdk_bad "$cfg: clips table is not 3 fighters x 17 clips or the depth band is not the walk object"; return 1; }
    [ "$(grep -Ec '^neon-rumble: fighter (banderas sheet 657x642, body 200,264 facing right|rainbird sheet 602x605, body 264,240 facing left|adler sheet 548x1108, body 328,252 facing left)$' <<< "$out")" -eq 3 ] || {
        sdk_bad "$cfg: roster from game.bundle: a fighter sheet or spawn line is missing"; return 1; }
    [ "$(grep -Ec '^neon-rumble: pose tick 0: ' <<< "$out")" -eq 2 ] && [ "$(grep -Ec '^neon-rumble: pose tick 0: (adler/idle frame 0 flip 1|banderas/idle frame 0 flip 0), 0 hit, 1 hurt, 1 push, 19 overlay quad\(s\), 0 rejected, 0 dropped$' <<< "$out")" -eq 2 ] || {
        sdk_bad "$cfg: roster poses on tick 0 are not banderas and adler idle with one hurt box, one push box and an overlay"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (brawl tick 0: hash ec2978dfca11282e, banderas 200,264 y 0 hp 100, adler 328,252 y 0 hp 100, draw adler banderas|seat tick 2: P2 joining|seat tick 3: P2 present|react tick 31: rainbird block|react tick 41: rainbird stand|hit tick 53: banderas/jab -> adler, damage 6, hp 94|react tick 53: adler hurt|hit tick 66: banderas/jab -> adler, damage 6, hp 88|hit tick 75: banderas/cross -> adler, damage 8, hp 80|react tick 92: adler stand|react tick 101: banderas block|react tick 111: banderas stand|hit tick 154: banderas/jump_kick -> adler, damage 10, hp 70|react tick 154: adler fall|react tick 174: adler down|hit tick 198: banderas/jump_kick -> adler, damage 10, hp 60|react tick 209: adler getup|react tick 239: adler stand|hit tick 329: banderas/run_kick -> adler, damage 12, hp 48|react tick 329: adler fall|react tick 346: adler down|react tick 353: banderas dodge|react tick 376: adler getup|react tick 377: banderas stand|react tick 406: adler stand|hit tick 417: banderas/grab -> adler, damage 6, hp 42|react tick 417: adler thrown|react tick 436: adler down|react tick 466: adler getup|brawl tick 480: hash afa1b0c122f176a0, banderas 458,252 y 0 hp 100, rainbird 264,240 y 0 hp 100, adler 413,252 y 0 hp 42, draw rainbird banderas adler)$' <<< "$out")" -eq 30 ] && [ "$(grep -Ec '^neon-rumble: (hit|react|seat) tick ' <<< "$out")" -eq 28 ] || {
        sdk_bad "$cfg: P2 joining on tick 2, rainbird block, brawl hash, hits on adler, its hurt/fall/down/getup/thrown ticks, banderas block/dodge/grab, positions or draw order on tick 0 or after 480 scripted ticks moved"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (font monogram line 12, 390 glyph\(s\), atlas 224x156|credit (chewbatrij|monogram|puffolotti-bad-company|puffolotti-up2|warped-city) \| .*|credits screen 960x540 scale 2: 5 pack\(s\), 2 page\(s\), [1-9][0-9]* line\(s\), [1-9][0-9]* glyph\(s\), 0 unknown, [1-9][0-9]* quad\(s\), 0 dropped)$' <<< "$out")" -eq 7 ] || {
        sdk_bad "$cfg: font monogram, five packs of credits.txt or a two-page credits screen with 0 unknown and 0 dropped missing"; return 1; }
    sdk_ok "$cfg: built against the prefix and ran 480 headless frames"
}

# Режим --wave (В5б, В6д, В7д, В8д спеки #25): арены, волны, чекпоинты и ящик из объектов level1, камера
# драки и замок арены — фреймворка. Бот P1 идёт вправо, пока нет врагов, по пути разбивает ящик (440,248):
# `death` на тике 172, `break` — снятие в начале следующего. Чекпоинт `order 0` (600,248) взят на тике
# 281, арена 0 запирается, когда центр камеры доходит до 712. Тело с hp 0 снимается в начале
# следующего тика, а step_seats ставит место новым телом у взятого чекпоинта (600,248, а не на краю
# вида 552) с immune 120 — к концу тика уже 119. Волна 0 для одного игрока — две точки из трёх
# (третья `players 2`), волна 1 — через 120 тиков, затем замок снят. Чекпоинт `order 1` (1300,248) —
# на тике 4105; в арене 1 волна 1 только для двоих пропущена, после волны 0 стартует волна 4 со стрелком
# rainbird — его ножи бьют P1 (`knife#N/throw`), — замок снят на тике 7530, к 8640 P1 стоит на краю
# улицы. Удар по hp 0 и строка `death` того же тика пиннуты перед каждым из 19 нокаутов и разбитым
# ящиком: нокаут — от удара, смерть — факт фреймворка, а не снятие тела. Строка позы тика 0 — одна: у
# ящика нет клипа, и отчёт поз его пропускает.
game_wave_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --wave --frames 8640 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave exited non-zero"; return 1; }
    grep -E '^neon-rumble: (brawl|break|knockout|wave|arena|checkpoint) tick ' <<< "$out"
    grep -q '^neon-rumble: headless run ok, 8640 frames$' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "$cfg: --wave: no headless verdict line"; return 1; }
    [ "$(grep -c '^neon-rumble: pose tick 0: ' <<< "$out")" -eq 1 ] || {
        printf '%s\n' "$out"; sdk_bad "$cfg: --wave: the pose report took a body without a clip (the crate)"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (stage camera 296,172, 2 arena\(s\), 14 wave spawn\(s\)|brawl tick 0: hash aa57d1a1c0c7819d, banderas#1 200,264 y 0 hp 100, crate#2 440,248 y 0 hp 20, camera 296,172 arena -1, draw crate#2 banderas#1|death tick 172: crate#2|hit tick 172: banderas#1/kick -> crate#2, damage 12, hp 0|break tick 173: crate#2|checkpoint tick 281: order 0 at 600,248|arena tick 337: arena 0 locked|wave tick 337: arena 0 wave 0, adler#3 adler#4|death tick 814: banderas#1|hit tick 814: adler#3/jab -> banderas#1, damage 6, hp 0|knockout tick 815: banderas#1, back as banderas#5 at 600,248 immune 119|death tick 1470: banderas#5|hit tick 1470: adler#4/jab -> banderas#5, damage 6, hp 0|knockout tick 1471: banderas#5, back as banderas#6 at 600,248 immune 119|death tick 1813: adler#4|hit tick 1813: banderas#6/cross -> adler#4, damage 8, hp 0|knockout tick 1814: adler#4|death tick 2177: banderas#6|hit tick 2177: adler#3/jab -> banderas#6, damage 6, hp 0|knockout tick 2178: banderas#6, back as banderas#7 at 600,248 immune 119|death tick 2259: adler#3|hit tick 2259: banderas#7/jab -> adler#3, damage 6, hp 0|knockout tick 2260: adler#3|wave tick 2260: arena 0 wave 0 cleared|wave tick 2380: arena 0 wave 1, adler#8 adler#9 adler#10|death tick 3541: adler#8|hit tick 3541: banderas#7/cross -> adler#8, damage 8, hp 0|knockout tick 3542: adler#8|death tick 3632: banderas#7|hit tick 3632: adler#9/jab -> banderas#7, damage 6, hp 0|knockout tick 3633: banderas#7, back as banderas#11 at 600,248 immune 119|death tick 3681: adler#10|hit tick 3681: banderas#11/jab -> adler#10, damage 6, hp 0|knockout tick 3682: adler#10|death tick 3735: adler#9|hit tick 3735: banderas#11/jab -> adler#9, damage 6, hp 0|knockout tick 3736: adler#9|wave tick 3736: arena 0 wave 1 cleared|arena tick 3736: arena 0 released|checkpoint tick 4105: order 1 at 1300,248|arena tick 4139: arena 1 locked|wave tick 4139: arena 1 wave 0, adler#12 adler#13|death tick 5078: banderas#11|hit tick 5078: adler#12/jab -> banderas#11, damage 6, hp 0|knockout tick 5079: banderas#11, back as banderas#14 at 1300,248 immune 119|death tick 5151: adler#12|hit tick 5151: banderas#14/jab -> adler#12, damage 6, hp 0|knockout tick 5152: adler#12|death tick 5547: adler#13|hit tick 5547: banderas#14/jab -> adler#13, damage 6, hp 0|knockout tick 5548: adler#13|wave tick 5548: arena 1 wave 0 cleared|wave tick 5668: arena 1 wave 4, adler#15 adler#16 adler#17 rainbird#18|death tick 5825: banderas#14|hit tick 5825: knife#20/throw -> banderas#14, damage 8, hp 0|knockout tick 5826: banderas#14, back as banderas#21 at 1300,248 immune 119|death tick 6533: adler#15|hit tick 6533: banderas#21/cross -> adler#15, damage 8, hp 0|knockout tick 6534: adler#15|death tick 6619: banderas#21|hit tick 6619: adler#16/jab -> banderas#21, damage 6, hp 0|knockout tick 6620: banderas#21, back as banderas#30 at 1300,248 immune 119|death tick 7088: banderas#30|hit tick 7088: knife#36/throw -> banderas#30, damage 8, hp 0|knockout tick 7089: banderas#30, back as banderas#37 at 1300,248 immune 119|death tick 7177: adler#16|hit tick 7177: banderas#37/cross -> adler#16, damage 8, hp 0|knockout tick 7178: adler#16|death tick 7324: adler#17|hit tick 7324: banderas#37/jab -> adler#17, damage 6, hp 0|knockout tick 7325: adler#17|death tick 7529: rainbird#18|hit tick 7529: banderas#37/cross -> rainbird#18, damage 8, hp 0|knockout tick 7530: rainbird#18|wave tick 7530: arena 1 wave 4 cleared|arena tick 7530: arena 1 released|brawl tick 8640: hash fd1c9ddcfdecf409, banderas#37 1560,238 y 0 hp 76, camera 1368,172 arena -1, draw banderas#37)$' <<< "$out")" -eq 77 ] && [ "$(grep -Ec '^neon-rumble: (break|knockout|wave|arena|death|checkpoint) tick ' <<< "$out")" -eq 54 ] && [ "$(grep -Ec '^neon-rumble: (break|hit|react|seat|knockout|wave|arena|death|checkpoint) tick ' <<< "$out")" -eq 664 ] || {
        printf '%s\n' "$out"
        sdk_bad "$cfg: --wave: arena lock, waves and checkpoints from the level, respawn at the checkpoint with immune, death facts, the skipped two-player wave, the crate break, the brawl hash or the tick count of the fight moved"; return 1; }
    sdk_ok "$cfg: --wave ran 8640 headless frames: two arenas locked and released, waves and checkpoints from the level, P1 back at the taken checkpoint immune"
}

# Сработавший снаряд (В8д спеки #25): нож knife#19 стрелка попадает в P1 на тике 5734 и остаётся в пуле с
# life 0 до drop_spent следующего тика. Ни строка тел, ни порядок отрисовки его уже не содержат. Прогон до
# 5733 — позитивный контроль: тот же нож летит и стоит в обеих, иначе исчезновение ничего не судит.
game_knife_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --wave --frames 5733 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave --frames 5733 exited non-zero"; return 1; }
    grep -E '^neon-rumble: brawl tick 5733: ' <<< "$out"
    grep -qx 'neon-rumble: brawl tick 5733: hash ea2a46462ee0debc, banderas#14 1558,260 y 0 hp 46, rainbird#18 1492,258 y 0 hp 80, adler#15 1495,260 y 0 hp 80, adler#16 1479,250 y 0 hp 80, adler#17 1530,262 y 0 hp 100, knife#19 1534,258 y 0 hp 0, camera 1368,172 arena 1, draw adler#16 rainbird#18 knife#19 banderas#14 adler#15 adler#17' <<< "$out" || {
        printf '%s\n' "$out"
        sdk_bad "$cfg: --wave tick 5733: the flying knife is missing from the draw order or the body line"; return 1; }
    out=$("$exe" --headless --wave --frames 5734 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave --frames 5734 exited non-zero"; return 1; }
    grep -E '^neon-rumble: brawl tick 5734: ' <<< "$out"
    grep -qx 'neon-rumble: brawl tick 5734: hash e07c1f2eba670525, banderas#14 1558,260 y 0 hp 38, rainbird#18 1492,258 y 0 hp 80, adler#15 1495,261 y 0 hp 80, adler#16 1479,250 y 0 hp 80, adler#17 1532,262 y 0 hp 100, camera 1368,172 arena 1, draw adler#16 rainbird#18 banderas#14 adler#15 adler#17' <<< "$out" &&
        grep -qx 'neon-rumble: hit tick 5734: knife#19/throw -> banderas#14, damage 8, hp 38' <<< "$out" || {
        printf '%s\n' "$out"
        sdk_bad "$cfg: --wave tick 5734: the spent knife is still drawn or reported, or the knife hit moved"; return 1; }
    sdk_ok "$cfg: --wave ticks 5733 and 5734: knife#19 flies in the draw order and the body line, hits P1 and is gone from both"
}

# Сбой загрузки — возврат в меню, а не краш (В7д спеки #25): headless обходит меню через `--level`, и
# уровень, которого нет в бандле, печатает строку `menu:` с текстом `load_level` и выходит нулём, не
# дойдя до драки. Позитивный контроль — прогоны выше: `level1` той же дорогой загружается.
game_menu_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --level nope --frames 1 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: --level nope exited non-zero instead of returning to the menu"; return 1; }
    grep -qx 'neon-rumble: menu: no level nope in the visual table' <<< "$out" && ! grep -q 'headless run ok' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "$cfg: --level nope: no menu line with the load_level refusal, or the fight ran"; return 1; }
    sdk_ok "$cfg: --level nope returned to the menu with the load_level refusal"
}
