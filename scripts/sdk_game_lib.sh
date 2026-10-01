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
