# shellcheck shell=bash

owner_aseprite_size() {
    od -An -tu1 -j16 -N8 "$1" 2>/dev/null \
        | awk 'NF == 8 { printf "%dx%d\n", (($1 * 256 + $2) * 256 + $3) * 256 + $4, (($5 * 256 + $6) * 256 + $7) * 256 + $8 }'
}

owner_aseprite_sheet() {
    local size
    size=$(owner_aseprite_size "$1/queen.png")
    if [ "$size" = 592x300 ]; then
        printf 'ok    §19 %s: sheet 592x300\n' "$2"
        return 0
    fi
    printf 'FAIL  §19 %s: sheet %s, not 592x300 - By Rows with Columns 8\n' "$2" "${size:-missing}"
    return 1
}

owner_aseprite_judge() {
    local d=$1 what=$2 rc=0
    owner_aseprite_sheet "$d" "$what" || rc=1
    if bash scripts/aseprite_owner_check.sh "$d" > "$d/check.out" 2>&1; then
        printf 'ok    §19 %s: four rows ok, aseprite-export: PASS\n' "$what"
    else
        printf 'FAIL  §19 %s: %s\n' "$what" "$(grep -m1 'missing:\|refus\|FAIL\|no ' "$d/check.out")"
        rc=1
    fi
    return $rc
}

owner_aseprite_abs() {
    case $2 in
        /* | [A-Za-z]:*) printf '%s' "$2" ;;
        *) printf '%s/%s' "$1" "$2" ;;
    esac
}

owner_stage_aseprite() {
    local dir=$1 caller=$2 bin queen gui rc=0
    mkdir -p "$dir"
    if [ -z "${ASEPRITE_QUEEN:-}" ]; then
        owner_stage_skip "$dir" "ASEPRITE_QUEEN is unset - name the Queen.ase marked by steps 1-3 of §19"
        owner_stage_end "$dir" aseprite 19 $?
        return
    fi
    bin=${ASEPRITE:-aseprite}
    case $bin in
        */* | *\\*) bin=$(owner_aseprite_abs "$caller" "$bin") ;;
        *) bin=$(command -v "$bin") ;;
    esac
    [ -n "$bin" ] || [ "$(uname -s)" != Darwin ] || bin=/Applications/Aseprite.app/Contents/MacOS/aseprite
    queen=$(owner_aseprite_abs "$caller" "$ASEPRITE_QUEEN")
    if [ ! -f "$queen" ] || [ ! -f "$bin" ] || [ ! -x "$bin" ]; then
        printf 'FAIL  §19 no %s - set ASEPRITE_QUEEN to the marked Queen.ase and ASEPRITE to the aseprite binary\n' \
            "$([ -f "$queen" ] && echo "aseprite at '$bin'" || echo "$queen")" >> "$dir/verdict.txt"
        owner_stage_end "$dir" aseprite "" 1
        return
    fi
    mkdir -p "$dir/cli"
    if ! (cd "$dir/cli" && "$bin" -b "$queen" --sheet queen.png --sheet-type rows --sheet-columns 8 \
        --data queen.json --format json-array --list-tags --list-slices) > "$dir/export.out" 2>&1; then
        echo "FAIL  §19 aseprite -b exited non-zero, see export.out" >> "$dir/verdict.txt"
        rc=1
    fi
    owner_aseprite_judge "$dir/cli" "command-line export" >> "$dir/verdict.txt" || rc=1
    owner_stage_look "$dir" cli/queen.png "§19 - the command-line export: sheet 592x300, eight frames to a row"
    if [ -n "${ASEPRITE_GUI:-}" ]; then
        gui=$(owner_aseprite_abs "$caller" "$ASEPRITE_GUI")
        mkdir -p "$dir/gui"
        cp "$gui/queen.json" "$gui/queen.png" "$dir/gui/" 2>/dev/null
        owner_aseprite_judge "$dir/gui" "export from the menu" >> "$dir/verdict.txt" || rc=1
        owner_stage_look "$dir" gui/queen.png "§19 - the export from the menu: the same sheet as cli/queen.png"
    else
        echo "note  §19 ASEPRITE_GUI is unset - the export from the menu (step 4) is not judged" >> "$dir/verdict.txt"
    fi
    owner_stage_end "$dir" aseprite 19 $rc
}
