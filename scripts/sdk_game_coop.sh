# shellcheck shell=bash
# Хотсит Neon Rumble на двоих (спека #25 В9д): `--wave --coop` — оба места играют скрипт волны, P2
# входит нампадом дорогой лобби, как в окне. Двое проходят обе арены level1 (с волной `players 2`
# арены 1, которую прогон --wave одним игроком пропускает), оба чекпоинта и platform-секцию: P2 лента
# довозит до её конца 1760, P1 с ленты идёт дальше до 1977. Когда вид уже в секции, P2 выходит своей
# LEAVE (тик 6600) и входит снова (6660): точка входа — край зоны вида у P1 с отступом ENTRY_MARGIN
# (1977 - 192 + 32 = 1817), в секции и в виде, а не у её края за ним (`refusal_x`, В8д). В корпус P1
# вход здесь не ложится по построению — центр вида и есть P1, а точка на 160 левее; правило входа в
# корпус судит framework_brawl_section_test (В9б).

# Строки прогона 6720 кадров по порядку — места, входы, нокауты, смерти, волны, арены, чекпоинты, ящик
# и строки тел тиков 0 и 6720; сдвиг любой — находка детерминизма или правки шага.
COOP_GOLDEN=$(cat <<'EOF'
neon-rumble: brawl tick 0: hash ee25bc928a3ae1a9, banderas#1 200,264 y 0 hp 100, crate#2 440,248 y 0 hp 20, camera 296,172 arena -1, draw crate#2 banderas#1
neon-rumble: seat tick 2: P2 joining
neon-rumble: seat tick 3: P2 present
neon-rumble: join tick 3: P2 as rainbird#3 at 264,240 y 0 immune 119, view 104..488, belt
neon-rumble: death tick 133: crate#2
neon-rumble: break tick 134: crate#2
neon-rumble: checkpoint tick 242: order 0 at 600,248
neon-rumble: arena tick 300: arena 0 locked
neon-rumble: wave tick 300: arena 0 wave 0, adler#4 adler#5 adler#6
neon-rumble: death tick 574: banderas#1
neon-rumble: knockout tick 575: banderas#1, back as banderas#7 at 600,248 immune 119
neon-rumble: death tick 582: rainbird#3
neon-rumble: knockout tick 583: rainbird#3, back as rainbird#8 at 664,232 immune 119
neon-rumble: death tick 1199: rainbird#8
neon-rumble: knockout tick 1200: rainbird#8, back as rainbird#9 at 664,232 immune 119
neon-rumble: death tick 1393: adler#5
neon-rumble: knockout tick 1394: adler#5
neon-rumble: death tick 1459: banderas#7
neon-rumble: knockout tick 1460: banderas#7, back as banderas#10 at 600,248 immune 119
neon-rumble: death tick 1509: adler#6
neon-rumble: knockout tick 1510: adler#6
neon-rumble: death tick 1702: adler#4
neon-rumble: knockout tick 1703: adler#4
neon-rumble: wave tick 1703: arena 0 wave 0 cleared
neon-rumble: wave tick 1823: arena 0 wave 1, adler#11 adler#12 adler#13
neon-rumble: death tick 2161: rainbird#9
neon-rumble: knockout tick 2162: rainbird#9, back as rainbird#14 at 664,233 immune 119
neon-rumble: death tick 2655: banderas#10
neon-rumble: knockout tick 2656: banderas#10, back as banderas#15 at 600,248 immune 119
neon-rumble: death tick 2800: adler#12
neon-rumble: knockout tick 2801: adler#12
neon-rumble: death tick 2960: rainbird#14
neon-rumble: knockout tick 2961: rainbird#14, back as rainbird#16 at 664,232 immune 119
neon-rumble: death tick 3115: adler#11
neon-rumble: death tick 3115: adler#13
neon-rumble: knockout tick 3116: adler#11
neon-rumble: knockout tick 3116: adler#13
neon-rumble: wave tick 3116: arena 0 wave 1 cleared
neon-rumble: arena tick 3116: arena 0 released
neon-rumble: checkpoint tick 3433: order 1 at 1300,248
neon-rumble: arena tick 3515: arena 1 locked
neon-rumble: wave tick 3515: arena 1 wave 0, adler#17 adler#18
neon-rumble: death tick 4125: adler#18
neon-rumble: knockout tick 4126: adler#18
neon-rumble: death tick 4341: adler#17
neon-rumble: knockout tick 4342: adler#17
neon-rumble: wave tick 4342: arena 1 wave 0 cleared
neon-rumble: wave tick 4462: arena 1 wave 1, adler#19 adler#20
neon-rumble: death tick 4615: rainbird#16
neon-rumble: knockout tick 4616: rainbird#16, back as rainbird#21 at 1364,232 immune 119
neon-rumble: death tick 4626: banderas#15
neon-rumble: knockout tick 4627: banderas#15, back as banderas#22 at 1300,248 immune 119
neon-rumble: death tick 4846: adler#20
neon-rumble: knockout tick 4847: adler#20
neon-rumble: death tick 4855: adler#19
neon-rumble: knockout tick 4856: adler#19
neon-rumble: wave tick 4856: arena 1 wave 1 cleared
neon-rumble: wave tick 4976: arena 1 wave 4, adler#23 adler#24 adler#25 rainbird#26
neon-rumble: death tick 5265: rainbird#21
neon-rumble: knockout tick 5266: rainbird#21, back as rainbird#31 at 1364,232 immune 119
neon-rumble: death tick 5277: banderas#22
neon-rumble: knockout tick 5278: banderas#22, back as banderas#32 at 1300,248 immune 119
neon-rumble: death tick 5723: rainbird#31
neon-rumble: knockout tick 5724: rainbird#31, back as rainbird#38 at 1364,232 immune 119
neon-rumble: death tick 5739: adler#24
neon-rumble: knockout tick 5740: adler#24
neon-rumble: death tick 5890: adler#23
neon-rumble: knockout tick 5891: adler#23
neon-rumble: death tick 5899: adler#25
neon-rumble: knockout tick 5900: adler#25
neon-rumble: death tick 6128: rainbird#26
neon-rumble: knockout tick 6129: rainbird#26
neon-rumble: wave tick 6129: arena 1 wave 4 cleared
neon-rumble: arena tick 6129: arena 1 released
neon-rumble: seat tick 6600: P2 free
neon-rumble: seat tick 6660: P2 joining
neon-rumble: seat tick 6661: P2 present
neon-rumble: join tick 6661: P2 as rainbird#43 at 1817,232 y 0 immune 119, view 1705..2089, platform
neon-rumble: brawl tick 6720: hash 2f41b8de2b3932fa, banderas#32 1977,271 y 0 hp 84, rainbird#43 1817,271 y 0 hp 100, camera 1897,172 arena -1, draw banderas#32 rainbird#43
EOF
)

# Прогон 6599 — позитивный контроль секции до выхода: оба в ней, P2 на конце ленты, P1 за ней.
game_coop_run() {
    local dir=$1 cfg=$2 exe out got
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --wave --coop --frames 6599 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave --coop --frames 6599 exited non-zero"; return 1; }
    grep -qx 'neon-rumble: brawl tick 6599: hash 22bf467c7baead3a, banderas#32 1977,271 y 0 hp 84, rainbird#38 1760,271 y 0 hp 76, camera 1869,172 arena -1, draw banderas#32 rainbird#38' <<< "$out" || {
        grep -E '^neon-rumble: brawl tick 6599: ' <<< "$out"
        sdk_bad "$cfg: --coop tick 6599: both players are not in the platform section, P2 at the end of the belt"; return 1; }
    out=$("$exe" --headless --wave --coop --frames 6720 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble --wave --coop exited non-zero"; return 1; }
    got=$(grep -E '^neon-rumble: (break|knockout|wave|arena|death|checkpoint|seat|join|brawl) tick ' <<< "$out")
    grep -q '^neon-rumble: headless run ok, 6720 frames$' <<< "$out" && [ "$got" = "$COOP_GOLDEN" ] &&
        [ "$(grep -Ec '^neon-rumble: (break|hit|react|seat|join|knockout|wave|arena|death|checkpoint) tick ' <<< "$out")" -eq 822 ] || {
        diff <(printf '%s\n' "$COOP_GOLDEN") <(printf '%s\n' "$got")
        sdk_bad "$cfg: --coop: two players through both arenas, the two-player wave, checkpoints and the section, P2 leaving and joining back in view inside the section, or the tick count of the fight moved"
        return 1
    }
    sdk_ok "$cfg: --coop ran 6720 headless frames: two players through both arenas and the platform section, P2 joined back at 1817 in view inside the section"
}
