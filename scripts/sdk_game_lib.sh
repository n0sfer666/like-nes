# shellcheck shell=bash
# Помощники и сломанные фикстуры гейта check_sdk_game.sh (спека #24, В1). Вызывающий задаёт ROOT,
# WORK (временный каталог прогона), PREFIX (поставленный SDK) и GEN (генератор CMake).

sdk_ok() { printf 'sdk-game: OK   %s\n' "$1"; }
sdk_bad() { printf 'sdk-game: FAIL %s\n' "$1" >&2; }

# CMake на Windows — нативный и путь git-bash вида /d/a/… не понимает.
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi; }

# Конфигурирование и сборка игры против префикса; вывод — в <каталог>.log. --fresh — потому что
# каталог под --keep переживает прогон, а кеш с найденным вчера префиксом судил бы вчерашний.
game_build() {
    local src=$1 dir=$2 cfg=$3
    {
        cmake --fresh -S "$(native "$src")" -B "$(native "$dir")" -G "$GEN" -DCMAKE_BUILD_TYPE="$cfg" \
            -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" &&
            cmake --build "$(native "$dir")"
    } > "$dir.log" 2>&1
}

# Запуск --headless --frames 60: код выхода 0 И строка вердикта. Одного кода мало — exe, не
# дошедший до цикла кадров (скажем, без бандла рядом), тоже может выйти нулём из чужой ветки.
# Сводка уровня (В5б) — бандл прочитан игрой против SDK: таблица `visual`, RGBA8 тайлсета и кадр
# 960x540, весь легший в квады без чужих текстур и выпавших за край источников. Сводка бойца (В6б) —
# клипы, лист и спавн из бандла и поза витрины на тиках 0 и 215: клип, кадр и флип пинятся.
game_run() {
    local dir=$1 cfg=$2 exe out
    exe="$dir/neon_rumble"
    [ -f "$exe.exe" ] && exe="$exe.exe"
    out=$("$exe" --headless --frames 60 2>&1) || {
        printf '%s\n' "$out"; sdk_bad "$cfg: neon_rumble exited non-zero"; return 1; }
    printf '%s\n' "$out"
    grep -q '^neon-rumble: headless run ok, 60 frames' <<< "$out" || {
        sdk_bad "$cfg: no headless verdict line"; return 1; }
    grep -q '^neon-rumble: library.bundle [1-9][0-9]* bytes' <<< "$out" || {
        sdk_bad "$cfg: library.bundle next to the exe was not loaded"; return 1; }
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
    grep -q '^neon-rumble: fighter 20 clip(s), sheet 592x300, spawn 200,224 facing right$' <<< "$out" || {
        sdk_bad "$cfg: fighter from game.bundle: no summary line"; return 1; }
    grep -Eq '^neon-rumble: fighter tick 0: queen/Walk frame 0 flip 0, [0-9]+ hit, [0-9]+ hurt, [0-9]+ push, [1-9][0-9]* overlay quad\(s\), 0 rejected, 0 dropped$' \
        <<< "$out" || { sdk_bad "$cfg: fighter on tick 0 is not Walk frame 0 facing right"; return 1; }
    grep -Eq '^neon-rumble: fighter tick 215: queen/Jab frame 0 flip 1, [0-9]+ hit, [0-9]+ hurt, [0-9]+ push, [1-9][0-9]* overlay quad\(s\), 0 rejected, 0 dropped$' \
        <<< "$out" || { sdk_bad "$cfg: fighter on tick 215 is not Jab frame 0 flipped"; return 1; }
    [ "$(grep -Ec '^neon-rumble: (font monogram line 12, 390 glyph\(s\), atlas 224x156|credit (chewbatrij|monogram|puffolotti-bad-company|puffolotti-up2|warped-city) \| .*|credits screen 960x540 scale 2: 5 pack\(s\), 2 page\(s\), [1-9][0-9]* line\(s\), [1-9][0-9]* glyph\(s\), 0 unknown, [1-9][0-9]* quad\(s\), 0 dropped)$' <<< "$out")" -eq 7 ] || {
        sdk_bad "$cfg: font monogram, five packs of credits.txt or a two-page credits screen with 0 unknown and 0 dropped missing"; return 1; }
    sdk_ok "$cfg: built against the prefix and ran 60 headless frames"
}

# Копия игры, которую фикстура портит; исходник в дереве не трогается.
fixture_copy() {
    local name=$1
    rm -rf "${WORK:?}/fx-$name"
    cp -R "$ROOT/games/neon-rumble" "$WORK/fx-$name"
    printf '%s' "$WORK/fx-$name"
}

# Каждая фикстура обязана упасть по СВОЕЙ причине: отказ сборки без имени дефекта в выводе значил
# бы, что упало что-то другое, и фикстура перестала бы что-либо доказывать.
fixture_internal_header() {
    local src
    src=$(fixture_copy internal)
    printf '#include "renderer_internal.hpp"\n' >> "$src/src/rumble.cpp"
    if game_build "$src" "$WORK/fx-internal-build" Release; then
        sdk_bad "fixture: game including renderer_internal.hpp built against the prefix"; return 1
    fi
    grep -Eqi "renderer_internal\.hpp.*(not found|no such file)|C1083.*renderer_internal\.hpp" \
        "$WORK/fx-internal-build.log" || {
        tail -20 "$WORK/fx-internal-build.log"
        sdk_bad "fixture: internal include failed for another reason"; return 1; }
    sdk_ok "fixture: internal header is not in the prefix"
}

# Портится КОПИЯ префикса: под --keep настоящий переживает прогон, и прерванная между двумя mv
# фикстура оставила бы его без библиотеки. local PREFIX видят game_build и вложенные вызовы.
fixture_missing_library() {
    local lib victim f orig=$PREFIX
    local PREFIX="$WORK/fx-prefix"
    cp -R "$orig" "$PREFIX"
    lib="$PREFIX/lib/like-nes"
    victim=""
    for f in "$lib"/*framework_physics.*; do [ -f "$f" ] && victim=${f##*/}; done
    [ -n "$victim" ] || { sdk_bad "fixture: no framework_physics library in $lib"; return 1; }
    rm "$lib/$victim"
    if game_build "$ROOT/games/neon-rumble" "$WORK/fx-nolib-build" Release; then
        sdk_bad "fixture: prefix without $victim configured"; return 1
    fi
    # CMake переносит причину отказа find_package по ширине, и место переноса зависит от длины
    # пути префикса: фраза ищется в логе со схлопнутыми пробелами и переводами строк.
    tr -s '[:space:]' ' ' < "$WORK/fx-nolib-build.log" | grep -qF "missing library $victim" || {
        tail -20 "$WORK/fx-nolib-build.log"
        sdk_bad "fixture: missing $victim failed without naming it"; return 1; }
    sdk_ok "fixture: prefix without $victim is refused by find_package with its name"
}

# Только MSVC: без like_nes_add_game цель берёт динамический CRT по умолчанию CMake, а движок
# собран со статическим — компоновщик обязан отбить это LNK2038, а не собрать exe с двумя CRT.
fixture_msvc_crt() {
    local src
    src=$(fixture_copy crt)
    grep -v '^like_nes_add_game' "$src/CMakeLists.txt" > "$src/CMakeLists.new"
    mv "$src/CMakeLists.new" "$src/CMakeLists.txt"
    if game_build "$src" "$WORK/fx-crt-build" Debug; then
        sdk_bad "fixture: Debug game without like_nes_add_game linked on MSVC"; return 1
    fi
    grep -q "LNK2038" "$WORK/fx-crt-build.log" || {
        tail -20 "$WORK/fx-crt-build.log"
        sdk_bad "fixture: CRT mismatch failed without LNK2038"; return 1; }
    sdk_ok "fixture: Debug game without like_nes_add_game fails with LNK2038"
}

# bundle_hash из заголовка game.bundle рядом с exe (смещение 32, uint64 LE — все хосты LE) против
# закоммиченного bundle.hash. Читается копия рядом с exe, а не выход assetc: её грузит игра.
bundle_hash_check() {
    local dir=$1 cfg=$2 want_file=$3 got want
    [ -f "$dir/game.bundle" ] || { sdk_bad "$cfg: no game.bundle next to the exe"; return 1; }
    got=0x$(od -An -tx8 -j32 -N8 "$dir/game.bundle" | tr -d '[:space:]')
    want=$(tr -d '[:space:]' < "$want_file")
    [ "$got" = "$want" ] || {
        sdk_bad "$cfg: game.bundle bundle_hash $got, bundle.hash says $want (re-bake changed the bytes)"
        return 1; }
    sdk_ok "$cfg: game.bundle bundle_hash $got matches bundle.hash"
}

fixture_bundle_hash() {
    printf '0x0123456789abcdef\n' > "$WORK/fx-bundle.hash"
    if bundle_hash_check "$1" Release "$WORK/fx-bundle.hash" 2> "$WORK/fx-hash.log"; then
        sdk_bad "fixture: a wrong bundle.hash was accepted"; return 1
    fi
    grep -q "bundle.hash says 0x0123456789abcdef" "$WORK/fx-hash.log" || {
        cat "$WORK/fx-hash.log"; sdk_bad "fixture: wrong bundle.hash failed for another reason"; return 1; }
    sdk_ok "fixture: a wrong bundle.hash is refused"
}

fixture_manifest_no_codec() {
    local src
    src=$(fixture_copy nocodec)
    printf 'texture | street_tiles | assets/warped-city/tileset.png\n' > "$src/game.manifest"
    if game_build "$src" "$WORK/fx-nocodec-build" Release; then
        sdk_bad "fixture: a texture record without a codec baked"; return 1
    fi
    grep -q "game.manifest:1: texture record needs a codec" "$WORK/fx-nocodec-build.log" || {
        tail -20 "$WORK/fx-nocodec-build.log"
        sdk_bad "fixture: manifest without a codec failed for another reason"; return 1; }
    sdk_ok "fixture: texture record without a codec stops the game build with assetc's line"
}

# Перебейк судится по depfile: PNG и .tsj нет в DEPENDS, о них сборка знает только из depfile
# assetc. Без изменений assetc не зовётся; правка .tsj уровня или другой PNG — зовётся, и новый
# бандл доезжает до exe (хеш рядом с exe уже не тот, что был до правки). Свежесть правки — `sleep 2`,
# а не отметка из будущего: такая отметка делает вход новее любого выхода навсегда, и следующий
# раунд перебейка шёл бы при любом depfile. Две секунды — запас на ФС с секундными отметками.
# Раунд PNG идёт на манифесте без уровня: тайлсет сверяет размер картинки, а подмена другого размера.
fixture_rebake() {
    local src dir="$WORK/fx-rebake-build" tsj before
    src=$(fixture_copy rebake)
    tsj="$src/levels/warped-city.tsj"
    game_build "$src" "$dir" Release || {
        tail -20 "$dir.log"; sdk_bad "fixture: rebake copy did not build"; return 1; }
    rebake_round "$dir" again "" "a rebuild with nothing changed ran assetc again" || return 1
    sleep 2
    sed 's/"value":"ladder"/"value":"solid"/' "$tsj" > "$tsj.new" && mv "$tsj.new" "$tsj" || {
        sdk_bad "fixture: cannot edit the tileset in the rebake copy"; return 1; }
    rebake_round "$dir" tsj "Baking game.bundle" "an edited .tsj did not re-bake (depfile not honoured)" || return 1
    if bundle_hash_check "$dir" Release "$ROOT/games/neon-rumble/bundle.hash" > /dev/null 2>&1; then
        sdk_bad "fixture: the bundle re-baked from the edited .tsj did not reach the exe directory"; return 1
    fi
    sleep 2
    printf 'texture | street_tiles | pixel | assets/warped-city/tileset.png\n' > "$src/game.manifest"
    rebake_round "$dir" nolevel "Baking game.bundle" "a manifest without the level did not re-bake" || return 1
    before=$(od -An -tx8 -j32 -N8 "$dir/game.bundle" | tr -d '[:space:]')
    sleep 2
    cp "$ROOT/tools/assetc/assets/src/hero_albedo.png" "$src/assets/warped-city/tileset.png" || {
        sdk_bad "fixture: cannot replace the PNG in the rebake copy"; return 1; }
    rebake_round "$dir" png "Baking game.bundle" "a replaced PNG did not re-bake (depfile not honoured)" || return 1
    [ "$(od -An -tx8 -j32 -N8 "$dir/game.bundle" | tr -d '[:space:]')" != "$before" ] || {
        sdk_bad "fixture: the bundle re-baked from the replaced PNG did not reach the exe directory"; return 1; }
    sdk_ok "fixture: depfile re-bakes on an edited .tsj and a replaced PNG and only then, the bundle reaches the exe"
}

rebake_round() {
    local dir=$1 tag=$2 want=$3 why=$4
    cmake --build "$(native "$dir")" > "$dir.$tag.log" 2>&1 || {
        tail -20 "$dir.$tag.log"; sdk_bad "fixture: rebuild ($tag) failed"; return 1; }
    if [ -z "$want" ]; then
        grep -q "Baking game.bundle" "$dir.$tag.log" && { sdk_bad "fixture: $why"; return 1; }
        return 0
    fi
    grep -q "$want" "$dir.$tag.log" || { cat "$dir.$tag.log"; sdk_bad "fixture: $why"; return 1; }
}
