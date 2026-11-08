# shellcheck shell=bash

net_fixture_project() {
    local dir=$1
    mkdir -p "$dir" || return 1
    cp "$ROOT/engine/platform/platform_net_address_test.cpp" \
        "$ROOT/engine/net/"{net_loopback,net_lobby,net_session_wire,net_relay_path}_test.cpp "$dir" || return 1
    cat > "$dir/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.21)
project(network_only LANGUAGES CXX)
find_package(like-nes 0.1 REQUIRED CONFIG)
get_target_property(engine_links like-nes::engine INTERFACE_LINK_LIBRARIES)
if(like-nes::net_core IN_LIST engine_links OR like-nes::platform_net IN_LIST engine_links)
  message(FATAL_ERROR "network must stay separate from like-nes::engine")
endif()
foreach(test platform_net_address_test net_loopback_test net_lobby_test net_session_wire_test net_relay_path_test)
  add_executable(${test} ${test}.cpp)
  target_link_libraries(${test} PRIVATE like-nes::net_core Threads::Threads)
  if(COMMAND like_nes_add_game)
    like_nes_add_game(${test})
  endif()
endforeach()
CMAKE
}

build_net_fixture() {
    local dir=$1 cfg=$2 prefix=$3 log
    shift 3
    log="$dir/$cfg.log"
    if ! cmake -S "$(native "$dir")" -B "$(native "$dir/$cfg")" -G "$GEN" -DCMAKE_BUILD_TYPE="$cfg" \
        -Dlike-nes_DIR="$(native "$prefix/lib/cmake/like-nes")" "$@" > "$log" 2>&1 ||
        ! cmake --build "$(native "$dir/$cfg")" >> "$log" 2>&1; then
        tail -30 "$log"
        return 1
    fi
}

fixture_net() {
    local dir="$WORK/network" cfg log
    net_fixture_project "$dir" || return 1
    for cfg in Release Debug; do
        build_net_fixture "$dir" "$cfg" "$PREFIX" || {
            sdk_bad "net: $cfg installed consumer failed to compile/link"; return 1; }
        log="$dir/$cfg.log"
        if ! "$dir/$cfg/platform_net_address_test" >> "$log" 2>&1 ||
            ! "$dir/$cfg/net_loopback_test" >> "$log" 2>&1 ||
            ! "$dir/$cfg/net_lobby_test" >> "$log" 2>&1 ||
            ! "$dir/$cfg/net_session_wire_test" >> "$log" 2>&1 ||
            ! "$dir/$cfg/net_relay_path_test" >> "$log" 2>&1 ||
            ! grep -qxF 'platform-net-address: PASS' "$log" ||
            ! grep -qxF 'net reliability over IPv6 loopback' "$log" ||
            ! grep -qxF 'net-loopback: PASS' "$log" ||
            ! grep -qxF 'net-lobby: PASS' "$log" ||
            ! grep -qxF 'net-session-wire: PASS' "$log" ||
            ! grep -qxF 'net-relay-path: PASS' "$log"; then
            tail -30 "$log"
            sdk_bad "net: $cfg installed consumer failed"
            return 1
        fi
    done
    sdk_ok 'net: installed network target runs IPv4/IPv6 UDP, resolver and lobby in Release/Debug'
}

fixture_mobile_net() {
    local plat=$1 prefix=$2 dir="$WORK/network-$1"
    shift 2
    if ! net_fixture_project "$dir" || ! build_net_fixture "$dir" Release "$prefix" "$@"; then
        sdk_bad "$plat net: installed network target failed to compile/link"
        return 1
    fi
    sdk_ok "$plat net: installed network archives compiled and linked (cross target not run)"
}

game_network_run() {
    local dir=$1 cfg=$2 fixture="$1/rumble_network_sim_test" exe="$1/neon_rumble"
    [ ! -f "$exe.exe" ] || exe="$exe.exe"
    if ! cmake --build "$dir" --target rumble_network_sim_test > "$dir/network-sim-build.log" 2>&1; then
        tail -30 "$dir/network-sim-build.log"
        sdk_bad "$cfg: network simulation fixture failed to build"
        return 1
    fi
    [ ! -f "$fixture.exe" ] || fixture="$fixture.exe"
    if ! "$fixture" "$dir/game.bundle" || ! python3 "$ROOT/scripts/check_network_coop.py" "$exe"; then
        sdk_bad "$cfg: Neon Rumble network fixture failed"
        return 1
    fi
    if [ "$cfg" = Release ]; then
        local relay="$ROOT/build-sdk-release/like_nes_relay"
        [ ! -f "$relay.exe" ] || relay="$relay.exe"
        if ! python3 "$ROOT/scripts/check_network_relay.py" "$exe" --daemon "$relay"; then
            sdk_bad "$cfg: live relay and direct-path fixture failed"
            return 1
        fi
    fi
    sdk_ok "$cfg: two real Neon Rumble peers and deterministic rollback/recovery scenarios"
}
