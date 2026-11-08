#include <chrono>
#include <cstdio>
#include <thread>

#include "gpu.hpp"
#include "mobile_app.hpp"
#include "rumble_net_mode.hpp"

namespace {

void tap(mobile::Game& game, float y, float x = 422) {
    game.pointer(1, mobile::Touch::Down, x, y);
    game.pointer(1, mobile::Touch::Up, x, y);
}

void cancel_relay_lookup(mobile::Game& game) {
    tap(game, 170.82f); // Edit relay address
    for (char c : std::string_view("127.0.0.1:7777")) {
        (void)c;
        tap(game, 327.6f, 253.2f); // Erase
    }
    for (char c : std::string_view("localhost:7777")) {
        const auto at = rumble::NETWORK_TEXT_KEYS.find(c);
        const float x = (0.10f + 0.08f * static_cast<float>(at % 11)) * 844;
        const float y = (0.36f + 0.11f * static_cast<float>(at / 11)) * 390;
        tap(game, y, x);
    }
    tap(game, 327.6f, 590.8f); // Done
    tap(game, 196.95f);       // Create relay room
    tap(game, 304.2f);        // Back before the game polls the result
}

std::string field(const mobile::Game& game, std::string_view key) {
    const auto state = game.touch_state();
    const size_t at = state.find(key);
    if (at == std::string::npos) return {};
    const size_t start = at + key.size();
    return state.substr(start, state.find(' ', start) - start);
}

} // namespace

bool network_touch() {
    using Clock = std::chrono::steady_clock;
    GpuContext gpu;
    auto host = mobile::make_game();
    auto guest = mobile::make_game();
    if (!host->init(gpu) || !guest->init(gpu)) return false;
    for (auto* game : {host.get(), guest.get()}) game->resize(2532, 1170, {44, 0, 44, 21, 3});
    cancel_relay_lookup(*guest);
    tap(*host, 66.3f);   // Host LAN
    tap(*guest, 144.7f); // Join the default loopback address

    const auto end = Clock::now() + std::chrono::seconds(8);
    auto pump = [&] {
        host->frame(1.0 / 60);
        guest->frame(1.0 / 60);
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    };
    auto fighting = [](const mobile::Game& game) {
        const auto tick = field(game, " tick=");
        return !tick.empty() && tick != "0" && !field(game, " player_x=").empty();
    };
    while (Clock::now() < end && (!fighting(*host) || !fighting(*guest))) {
        tap(*host, 265.2f); // Ready appears after the offer
        tap(*guest, 265.2f);
        pump();
    }
    const auto begin = field(*host, " player_x=");
    if (!fighting(*host) || !fighting(*guest)) {
        std::puts("network-touch: FAIL lobby did not start");
        return false;
    }

    host->pointer(2, mobile::Touch::Down, 732, 301); // claim the touch device
    for (int i = 0; i < 20; ++i) pump();
    host->pointer(2, mobile::Touch::Up, 732, 301);
    host->pointer(3, mobile::Touch::Down, 200, 240);
    host->pointer(3, mobile::Touch::Move, 248, 240);
    while (Clock::now() < end && field(*guest, " player_x=") == begin) pump();
    const bool moved = !field(*guest, " player_x=").empty() && field(*guest, " player_x=") != begin;
    host->pointer(3, mobile::Touch::Up, 248, 240);
    for (int i = 0; i < 40; ++i) pump();
    host->pointer(4, mobile::Touch::Down, 732, 301);
    bool jab = false;
    while (Clock::now() < end && !jab) {
        pump();
        jab = field(*guest, " pose=") == "banderas/jab";
    }
    host->pointer(4, mobile::Touch::Up, 732, 301);
    host->shutdown();
    guest->shutdown();
    if (moved && jab) std::puts("network-touch: PASS host tapped lobby, moved and attacked on guest");
    else std::printf("network-touch: FAIL moved=%d jab=%d\n", static_cast<int>(moved), static_cast<int>(jab));
    return moved && jab;
}
