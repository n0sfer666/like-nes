#include <cstdio>
#include <string>

#include "platform_io.hpp"
#include "rumble_credits.hpp"
#include "rumble_flow.hpp"
#include "rumble_hotseat.hpp"
#include "rumble_menu.hpp"
#include "rumble_roster.hpp"
#include "rumble_window_bench.hpp"

bool bench_touch(const std::string& assets) {
    platform::MappedFile file;
    asset::BundleView bundle;
    rumble::Credits credits;
    rumble::Menu menu;
    rumble::Hotseat hotseat;
    rumble::WindowBenchmark bench;
    rumble::Flow flow(bundle, credits, menu);
    std::string why;
    if (!file.open(assets + "/game.bundle") || !bundle.open(file.data(), file.size(), false) ||
        !credits.open(bundle) || !menu.init("level1", false) || !flow.open() ||
        !hotseat.open(true) || !bench.bind_touch(bundle, hotseat, why)) return false;
    flow.enter();
    flow.apply();
    auto* play = flow.play();
    if (!play) return false;
    fix32 first_x;
    bool joined = false, moved = false, attacked = false;
    for (uint32_t tick = 0; tick < 90; ++tick) {
        bench.feed(960, 540, play->scene.ticks);
        if (!hotseat.tick(tick + 1, play->brawl.players)) return false;
        flow.run(tick);
        const auto* player = play->brawl.pool.find(play->brawl.seats.at[rumble::PLAYER].body);
        if (!player || hotseat.touch_player() != static_cast<int>(rumble::PLAYER)) continue;
        if (!joined) first_x = player->pos.x;
        joined = true;
        moved |= player->pos.x != first_x;
        attacked |= player->move != framework::brawl::NO_STRIKE;
    }
    if (joined && moved && attacked) std::puts("bench-touch: PASS scripted touch joined, moved and attacked");
    return joined && moved && attacked;
}
