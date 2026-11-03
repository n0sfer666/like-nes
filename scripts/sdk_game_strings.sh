# shellcheck shell=bash
# Настоящий like_nes_bake из установленного SDK: Debug допускает fallback, release-конфиги — нет.
fixture_strings_bake() {
    local dir="$WORK/strings-bake" cfg log
    mkdir -p "$dir"
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(strings_bake CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
add_executable(strings_game main.cpp)
like_nes_bake(strings_game MANIFEST game.manifest OUT labels.bundle)
CMAKE
    printf 'int main() { return 0; }\n' > "$dir/main.cpp"
    printf 'strings|labels|tsv|labels.tsv|mono\nfont|mono|bitmask|mono.json\n' > "$dir/game.manifest"
    printf '{"A":[1],"?":[1]}\n' > "$dir/mono.json"
    printf 'key\ten\tru\nmenu.go\tA\t\n' > "$dir/labels.tsv"
    for cfg in Debug Release RelWithDebInfo MinSizeRel; do
        log="$dir/$cfg.log"
        if ! cmake -S "$(native "$dir")" -B "$(native "$dir/$cfg")" -G "$GEN" \
            -DCMAKE_BUILD_TYPE="$cfg" -DCMAKE_PREFIX_PATH="$(native "$PREFIX")" > "$log" 2>&1; then
            tail -20 "$log"; sdk_bad "strings: $cfg configure failed"; return 1
        fi
        if [ "$cfg" = Debug ]; then
            if ! cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1 ||
                ! grep -Fq 'strings labels: keys=1 fallbacks=1' "$log"; then
                tail -20 "$log"; sdk_bad 'strings: Debug did not allow a counted fallback'; return 1
            fi
        else
            if cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1; then
                sdk_bad "strings: $cfg allowed a fallback"; return 1
            fi
            if ! grep -Fq "key 'menu.go' falls back to en under --strict" "$log"; then
                tail -20 "$log"; sdk_bad "strings: $cfg failed for another reason"; return 1
            fi
        fi
    done
    printf 'key\ten\tru\nmenu.go\tA\tA\n' > "$dir/labels.tsv"
    if ! cmake --build "$(native "$dir/Release")" >> "$dir/complete.log" 2>&1 ||
        ! grep -Fq 'strings labels: keys=1 fallbacks=0' "$dir/complete.log"; then
        tail -20 "$dir/complete.log"; sdk_bad 'strings: completed translation did not rebuild'; return 1
    fi
    sdk_ok 'strings: Debug counts fallback; Release/RelWithDebInfo/MinSizeRel refuse; translation repair rebuilds'
}
