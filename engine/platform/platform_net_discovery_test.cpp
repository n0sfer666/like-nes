#include "platform_net_discovery.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using namespace platform::net;
    using Clock = std::chrono::steady_clock;
    const auto begin = Clock::now();
    constexpr char NAME[] = "likenes discovery fixture";
    constexpr uint16_t PORT = 46127;
    Discovery advertiser, browser;
    if (!browser.open({{}, 0, true}) || !advertiser.open({NAME, PORT, true})) {
        std::fprintf(stderr, "platform-discovery: failed to open LocalOnly publisher/browser\n");
        return 1;
    }
    bool registered = false;
    for (;;) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - begin).count();
        if (elapsed > 6'000) {
            std::fprintf(stderr, "platform-discovery: timed out (%s)\n", registered ? "remove" : "resolve");
            return 1;
        }
        const auto now = static_cast<uint64_t>(elapsed);
        advertiser.poll(now);
        browser.poll(now);
        if (!browser.available()) return 1;
        bool found = false;
        for (const auto& room : browser.rooms()) {
            if (room.name != NAME || room.endpoint.port != PORT) continue;
            const auto text = endpoint_text(room.endpoint);
            found = text == "127.0.0.1:46127" || text == "[::1]:46127";
        }
        if (found && !registered) {
            registered = true;
            advertiser.close();
        } else if (registered && !found) {
            std::puts("platform-discovery: PASS");
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
