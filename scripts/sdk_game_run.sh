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

# Режим --wave (В5б, В6д, В7д спеки #25): арены, волны и чекпоинты из объектов level1, камера драки и
# замок арены — фреймворка. Бот P1 идёт вправо, пока нет врагов; чекпоинт `order 0` (600,248) взят на
# тике 200, арена 0 запирается, когда центр камеры доходит до 712. Тело с hp 0 снимается в начале
# следующего тика, а step_seats ставит место новым телом у взятого чекпоинта (600,248, а не на краю
# вида 552) с immune 120 — к концу тика уже 119. Волна 0 для одного игрока — две точки из трёх
# (третья `players 2`), волна 1 — через 120 тиков, затем замок снят. Чекпоинт `order 1` (1300,248) —
# на тике 5389; в арене 1 волна 1 только для двоих пропущена, после волны 0 стартует волна 4, замок
# снят на тике 8560 — отсюда 8640 кадров. Удар по hp 0 и строка `death` того же тика пиннуты перед
# каждым из 19 нокаутов: нокаут — от удара, смерть — факт фреймворка, а не снятие тела.
game_wave_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --wave --frames 8640 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave exited non-zero"; return 1; }
    grep -E '^neon-rumble: (brawl|knockout|wave|arena|checkpoint) tick ' <<< "$out"
    grep -q '^neon-rumble: headless run ok, 8640 frames$' <<< "$out" || {
        printf '%s\n' "$out"; sdk_bad "$cfg: --wave: no headless verdict line"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (stage camera 296,172, 2 arena\(s\), 13 wave spawn\(s\)|brawl tick 0: hash a1b2e5abce5125e6, banderas#1 200,264 y 0 hp 100, camera 296,172 arena -1, draw banderas#1|checkpoint tick 200: order 0 at 600,248|arena tick 256: arena 0 locked|wave tick 256: arena 0 wave 0, adler#2 adler#3|death tick 749: banderas#1|hit tick 749: adler#2/jab -> banderas#1, damage 6, hp 0|knockout tick 750: banderas#1, back as banderas#4 at 600,248 immune 119|death tick 1545: banderas#4|hit tick 1545: adler#2/jab -> banderas#4, damage 6, hp 0|knockout tick 1546: banderas#4, back as banderas#5 at 600,248 immune 119|death tick 1887: adler#2|hit tick 1887: banderas#5/jab -> adler#2, damage 6, hp 0|knockout tick 1888: adler#2|death tick 2271: adler#3|hit tick 2271: banderas#5/jab -> adler#3, damage 6, hp 0|knockout tick 2272: adler#3|wave tick 2272: arena 0 wave 0 cleared|wave tick 2392: arena 0 wave 1, adler#6 adler#7 adler#8|death tick 2546: banderas#5|hit tick 2546: adler#7/jab -> banderas#5, damage 6, hp 0|knockout tick 2547: banderas#5, back as banderas#9 at 600,248 immune 119|death tick 3094: banderas#9|hit tick 3094: adler#8/kick -> banderas#9, damage 12, hp 0|knockout tick 3095: banderas#9, back as banderas#10 at 600,248 immune 119|death tick 3723: banderas#10|hit tick 3723: adler#6/jab -> banderas#10, damage 6, hp 0|knockout tick 3724: banderas#10, back as banderas#11 at 600,248 immune 119|death tick 4411: banderas#11|hit tick 4411: adler#7/jab -> banderas#11, damage 6, hp 0|knockout tick 4412: banderas#11, back as banderas#12 at 600,248 immune 119|death tick 4479: adler#8|hit tick 4479: banderas#12/jab -> adler#8, damage 6, hp 0|knockout tick 4480: adler#8|death tick 4750: adler#7|hit tick 4750: banderas#12/jab -> adler#7, damage 6, hp 0|knockout tick 4751: adler#7|death tick 4988: adler#6|hit tick 4988: banderas#12/jab -> adler#6, damage 6, hp 0|knockout tick 4989: adler#6|wave tick 4989: arena 0 wave 1 cleared|arena tick 4989: arena 0 released|checkpoint tick 5389: order 1 at 1300,248|arena tick 5423: arena 1 locked|wave tick 5423: arena 1 wave 0, adler#13 adler#14|death tick 5923: banderas#12|hit tick 5923: adler#14/jab -> banderas#12, damage 6, hp 0|knockout tick 5924: banderas#12, back as banderas#15 at 1300,248 immune 119|death tick 6370: adler#13|hit tick 6370: banderas#15/jab -> adler#13, damage 6, hp 0|knockout tick 6371: adler#13|death tick 6767: banderas#15|hit tick 6767: adler#14/kick -> banderas#15, damage 12, hp 0|knockout tick 6768: banderas#15, back as banderas#16 at 1300,248 immune 119|death tick 6964: adler#14|hit tick 6964: banderas#16/jab -> adler#14, damage 6, hp 0|knockout tick 6965: adler#14|wave tick 6965: arena 1 wave 0 cleared|wave tick 7085: arena 1 wave 4, adler#17 adler#18 adler#19|death tick 7935: adler#18|hit tick 7935: banderas#16/jab -> adler#18, damage 6, hp 0|knockout tick 7936: adler#18|death tick 8061: banderas#16|hit tick 8061: adler#17/jab -> banderas#16, damage 6, hp 0|knockout tick 8062: banderas#16, back as banderas#20 at 1300,248 immune 119|death tick 8481: adler#19|hit tick 8481: banderas#20/jab -> adler#19, damage 6, hp 0|knockout tick 8482: adler#19|death tick 8559: adler#17|hit tick 8559: banderas#20/jab -> adler#17, damage 6, hp 0|knockout tick 8560: adler#17|wave tick 8560: arena 1 wave 4 cleared|arena tick 8560: arena 1 released|brawl tick 8640: hash 425dbe982697dd98, banderas#20 1376,262 y 0 hp 64, camera 1368,172 arena -1, draw banderas#20)$' <<< "$out")" -eq 74 ] && [ "$(grep -Ec '^neon-rumble: (knockout|wave|arena|death|checkpoint) tick ' <<< "$out")" -eq 52 ] && [ "$(grep -Ec '^neon-rumble: (hit|react|seat|knockout|wave|arena|death|checkpoint) tick ' <<< "$out")" -eq 674 ] || {
        printf '%s\n' "$out"
        sdk_bad "$cfg: --wave: arena lock, waves and checkpoints from the level, respawn at the checkpoint with immune, death facts, the skipped two-player wave, the brawl hash or the tick count of the fight moved"; return 1; }
    sdk_ok "$cfg: --wave ran 8640 headless frames: two arenas locked and released, waves and checkpoints from the level, P1 back at the taken checkpoint immune"
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
