# shellcheck shell=bash
# Пекарь игры для like_nes_bake(TOOL …) в гейте sdk-game (спека #29, В2а): сборка rumble_assetc
# против десктопного префикса и фикстуры, на которых отказывает like_nes_bake.

# Собирается лениво, первой мобильной платформой: на машине без мобильной половины (Linux без NDK)
# пекарь хоста никому не нужен, а его сборка — лишние минуты и лишняя строка в выводе. Провал
# запоминается: вторая платформа не пересобирает его минутами ради второй такой же строки FAIL.
mobile_tool() {
    [ -z "${RUMBLE_TOOL:-}" ] || return 0
    [ -z "${RUMBLE_TOOL_FAILED:-}" ] || return 1
    RUMBLE_TOOL=$(bash "$ROOT/scripts/rumble_tool.sh" "$PREFIX" "$OUT/rumble-tool") || {
        RUMBLE_TOOL_FAILED=1; sdk_bad "rumble_assetc did not build against the desktop prefix"; return 1; }
    sdk_ok "rumble_assetc built against the desktop prefix"
}

# Пекарь со штампом другой сборки движка печёт бандл чужим ядром: конфигурирование игры обязано
# отбить его штампом, а не пропустить до бейка.
fixture_mobile_stale_tool() {
    local plat=$1 pfx=$2 tool="$WORK/fx-stale-tool/rumble_assetc" log="$WORK/fx-stale-$1.log"
    shift 2
    { mkdir -p "$WORK/fx-stale-tool" && cp "$RUMBLE_TOOL" "$tool" && echo "0.0.0+stale" > "$tool.like-nes-build"; } || {
        sdk_bad "fixture: cannot stage the stale rumble_assetc"; return 1; }
    if cmake --fresh -S "$ROOT/games/neon-rumble" -B "$WORK/fx-stale-$plat" -G "$GEN" -DCMAKE_BUILD_TYPE=Release \
        "$@" -Dlike-nes_DIR="$pfx/lib/cmake/like-nes" -DLIKE_NES_HOST_PREFIX="$PREFIX" -DRUMBLE_ASSETC="$tool" \
        > "$log" 2>&1; then
        sdk_bad "fixture: $plat game configured with a rumble_assetc of another engine build"; return 1
    fi
    tr -s '[:space:]' ' ' < "$log" | grep -q "built against like-nes build '0.0.0+stale'" || {
        tail -20 "$log"; sdk_bad "fixture: $plat stale rumble_assetc failed for another reason"; return 1; }
    sdk_ok "fixture: $plat game refuses a rumble_assetc of another engine build"
}

# Каждый отказ TOOL — своё конфигурирование мини-проекта против префикса: пустое значение или
# NOTFOUND молча откатились бы на assetc SDK, а путь без штампа или с чужим штампом испёк бы бандл
# чужим ядром. Отказ судится своим текстом, иначе любой сломанный проект сошёл бы за отказ.
fixture_bake_tool_refusals() {
    local src="$WORK/fx-baketool" mode label want fail=0
    { mkdir -p "$src/tool" && printf 'int main() { return 0; }\n' > "$src/main.cpp" &&
        printf 'texture|a|pixel|a.png\n' > "$src/game.manifest" &&
        printf 'fake\n' > "$src/tool/fake" && cp "$src/tool/fake" "$src/tool/stale" &&
        echo "0.0.0+stale" > "$src/tool/stale.like-nes-build"; } || {
        sdk_bad "fixture: cannot stage the like_nes_bake TOOL project"; return 1; }
    cat > "$src/CMakeLists.txt" << 'EOF'
cmake_minimum_required(VERSION 3.21)
project(bake_tool CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
add_executable(bake_tool main.cpp)
set(bake bake_tool MANIFEST game.manifest OUT game.bundle)
if(MODE STREQUAL "novalue")
  like_nes_bake(${bake} TOOL)
elseif(MODE STREQUAL "empty")
  like_nes_bake(${bake} TOOL "")
elseif(MODE STREQUAL "notfound")
  like_nes_bake(${bake} TOOL "RUMBLE_ASSETC-NOTFOUND")
elseif(MODE STREQUAL "relative")
  like_nes_bake(${bake} TOOL "tool/fake")
elseif(MODE STREQUAL "dir")
  like_nes_bake(${bake} TOOL "${CMAKE_CURRENT_SOURCE_DIR}/tool")
elseif(MODE STREQUAL "nostamp")
  like_nes_bake(${bake} TOOL "${CMAKE_CURRENT_SOURCE_DIR}/tool/fake")
elseif(MODE STREQUAL "stale")
  like_nes_bake(${bake} TOOL "${CMAKE_CURRENT_SOURCE_DIR}/tool/stale")
endif()
EOF
    while IFS='|' read -r mode label want; do
        if cmake --fresh -S "$(native "$src")" -B "$(native "$src/b-$mode")" -G "$GEN" -DMODE="$mode" \
            -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$src/$mode.log" 2>&1 < /dev/null; then
            sdk_bad "fixture: like_nes_bake accepted TOOL $label"; fail=1; continue
        fi
        tr -s '[:space:]' ' ' < "$src/$mode.log" | grep -qF "$want" || {
            tail -20 "$src/$mode.log"; sdk_bad "fixture: like_nes_bake TOOL $label failed for another reason"
            fail=1; continue; }
        sdk_ok "fixture: like_nes_bake refuses TOOL $label"
    done << 'EOF'
novalue|without a value|TOOL needs a target or an absolute path, got ''
empty|""|TOOL needs a target or an absolute path, got ''
notfound|*-NOTFOUND|TOOL needs a target or an absolute path, got 'RUMBLE_ASSETC-NOTFOUND'
relative|as a relative path|TOOL 'tool/fake' is neither a target nor an existing absolute path
dir|as a directory|tool' is neither a target nor an existing absolute path
nostamp|without a .like-nes-build stamp|.like-nes-build stamp; build it against the desktop prefix
stale|stamped by another engine build|was built against like-nes build '0.0.0+stale'
EOF
    return "$fail"
}
