# shellcheck shell=bash

OWNER_G8_GOLDEN=example_ugly_game/golden/scene_960x540.png
OWNER_G12_GOLDEN=engine/render/golden/lights_640x360.png
OWNER_G12_UNLIT=engine/render/golden/materials_640x360.png

owner_gpu_has() {
    if grep -qF -- "$3" "$2"; then
        printf 'ok    %s %s\n' "$1" "$3"
    else
        printf 'FAIL  %s no "%s"\n' "$1" "$3"
        return 1
    fi
}

owner_gpu_judge_g8() {
    local out=$1 rc=0 line adapter
    for line in "golden control: PASS" "golden repeat: PASS" "golden frame: PASS"; do
        owner_gpu_has §8 "$out" "$line" || rc=1
    done
    grep -E 'golden (repeat|frame): mean' "$out" | sed 's/^/      /'
    adapter=$(sed -n 's/^\[gpu\] \(.*\) | BC: .*/\1/p' "$out" | head -1)
    if [ -z "$adapter" ]; then
        printf 'FAIL  §8 no [gpu] line - the adapter is unknown\n'
        return 1
    fi
    case $(printf '%s' "$adapter" | tr '[:upper:]' '[:lower:]') in
        *amd* | *radeon*) printf 'ok    §8 adapter %s - the AMD answer the gate waits for\n' "$adapter" ;;
        *) printf 'note  §8 adapter %s - not AMD, §8 stays open for an AMD machine\n' "$adapter" ;;
    esac
    return $rc
}

owner_gpu_judge_g12() {
    local out=$1 pinned=$2 rc=0 line
    for line in "lights: 5 source(s) in the table, 5 uploaded" \
        "normals: 5 mapped from the table, 2 flat, 0 naming an unknown asset" \
        "occluders: 4 mapped from the table, 3 open, 0 naming an unknown asset" "light-gpu: PASS"; do
        owner_gpu_has §12 "$out" "$line" || rc=1
    done
    [ -n "$pinned" ] || return $rc
    if grep -qF "light-gpu: PASS" "$pinned" && grep -q 'golden .*: mean=' "$pinned"; then
        printf 'ok    §12 against the Metal reference: %s\n' "$(grep -o 'mean=.*' "$pinned")"
    else
        printf 'FAIL  §12 against the Metal reference: %s\n' \
            "$(grep -o 'mean=.*' "$pinned" || grep -m1 'golden written' "$pinned" || echo 'no numbers')"
        rc=1
    fi
    return $rc
}

owner_gpu_exe() {
    if [ -f "build/$1.exe" ]; then printf '%s/build/%s.exe' "$PWD" "$1"; else printf '%s/build/%s' "$PWD" "$1"; fi
}

owner_gpu_native() {
    if command -v cygpath > /dev/null 2>&1; then cygpath -m "$1"; else printf '%s' "$1"; fi
}

owner_stage_gpu() {
    local dir=$1 rc=0 pinned="" golden exe
    mkdir -p "$dir"
    if [ ! -f build/CMakeCache.txt ]; then
        owner_stage_skip "$dir" 'build/ is not configured - make build (Windows: scripts\win-dev.bat build)'
        owner_stage_end "$dir" gpu "8 12" $?
        return
    fi
    if ! cmake --build build --target game_sidescroller light_golden > "$dir/build.log" 2>&1; then
        echo "FAIL  build of game_sidescroller and light_golden, see build.log" >> "$dir/verdict.txt"
        owner_stage_end "$dir" gpu "" 1
        return
    fi
    golden=$(owner_gpu_native "$PWD/$OWNER_G8_GOLDEN")
    exe=$(owner_gpu_exe game_sidescroller)
    (cd "$dir" && "$exe" --frames 240 --golden "$golden" --golden-selftest > g8.out 2>&1) \
        || echo "FAIL  §8 game_sidescroller exited $?" >> "$dir/verdict.txt"
    if [ -f "$dir/golden_actual.png" ]; then
        mv "$dir/golden_actual.png" "$dir/g8-actual.png"
        owner_stage_look "$dir" g8-actual.png "§8 - the frame that missed $OWNER_G8_GOLDEN: send it with the numbers, do not re-bake"
    fi
    "$(owner_gpu_exe light_golden)" --golden "$dir/lights.png" > "$dir/g12.out" 2>&1 \
        || echo "FAIL  §12 light_golden exited $?" >> "$dir/verdict.txt"
    if [ "$(uname -s)" = Darwin ]; then
        pinned=$dir/g12-metal.out
        mkdir -p "$dir/ref"
        cp "$OWNER_G12_GOLDEN" "$dir/ref/"
        "$(owner_gpu_exe light_golden)" --golden "$dir/ref/$(basename "$OWNER_G12_GOLDEN")" > "$pinned" 2>&1
    fi
    owner_run_strip_cr "$dir/g8.out" "$dir/g12.out" ${pinned:+"$pinned"}
    owner_gpu_judge_g8 "$dir/g8.out" >> "$dir/verdict.txt" || rc=1
    owner_gpu_judge_g12 "$dir/g12.out" "$pinned" >> "$dir/verdict.txt" || rc=1
    grep -q '^FAIL' "$dir/verdict.txt" && rc=1
    cp "$OWNER_G12_UNLIT" "$dir/materials-unlit.png"
    owner_stage_look "$dir" lights.png "§12 - answer the eight questions of §12 by engine/light/library/lights.txt: warm top left and cold top right, haze only near rim, sun flat on the two right columns, domes lit from above, ribs vertical, streaks away from each source (question 7: fill sits inside the disc, an open item), flash casts nothing"
    owner_stage_look "$dir" materials-unlit.png "§12 question 1 - the same scene unlit (reference of §9): every sprite of lights.png is dimmer, and by a different amount"
    owner_stage_end "$dir" gpu "8 12" $rc
}
