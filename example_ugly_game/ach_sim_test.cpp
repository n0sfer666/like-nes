#include <cstdio>
#include <cstdlib>
#include <string>

#include "achievements.hpp"
#include "combat.hpp"
#include "sim.hpp"
#include "world.hpp"

// Гейт 7 (спека #10): достижения — НАБЛЮДАТЕЛЬ. sim их не вызывает, поэтому golden
// sim-хеш обязан совпасть и с трекером, и с загруженным плагином-бэкендом, и без них.

namespace {

constexpr uint64_t GOLDEN = 0x32a094e89eacf2f2ull;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("  FAIL %s\n", what);
    }
}

uint64_t scripted(int ticks, game::Achievements* ach, game::GameState* out) {
    flecs::world w;
    game::GameState gs;
    game::spawn(w, gs);
    const fix32 dt = fix32::from_float(1.0 / 60);
    static const double AX[4] = {0.6, 0.6, -0.35, 0.2};
    static const double AY[4] = {0.5, -0.5, 0.3, -0.6};
    for (int t = 0; t < ticks; ++t) {
        input::InputFrame f;
        f.tick = static_cast<uint32_t>(t);
        f.held = 1ull << game::A_Fire;
        if (t == 0) f.pressed = 1ull << game::A_Fire;
        const int ph = (t / 40) % 4;
        f.axes[game::AX_MoveX] = fix32::from_float(AX[ph]);
        f.axes[game::AX_MoveY] = fix32::from_float(AY[ph]);
        game::step(w, gs, f, dt);
        if (ach != nullptr) {
            ach->observe(gs);
            if ((t % 60) == 0) ach->pump();
        }
    }
    if (out != nullptr) *out = gs;
    return game::sim_hash(w, gs);
}

} // namespace

int main(int argc, char** argv) {
    std::printf("game achievements observer\n");
    const std::string bundle = argc > 1 ? argv[1] : "";
    const std::string plugin = argc > 2 ? argv[2] : "";
    const std::string save = "ach_sim_test.save";
    std::remove(save.c_str());

    game::GameState bare_state{};
    const uint64_t bare = scripted(1200, nullptr, &bare_state);
    check(bare == GOLDEN, "baseline hash is the spec #8 golden");

    game::Achievements a;
    a.init(bundle, save, "");
    game::GameState observed_state{};
    const uint64_t observed = scripted(1200, &a, &observed_state);
    check(observed == bare, "observer does not touch the sim");
    check(a.defined_count() > 0, "achievements loaded from the bundle");
    check(a.unlocked_count() > 0, "scripted run unlocks something");
    check(observed_state.kills == bare_state.kills && observed_state.score == bare_state.score,
          "game state identical");
    a.save();

    game::Achievements b;
    b.init(bundle, save, plugin);
    check(b.unlocked_count() == a.unlocked_count(), "progress restored from disk");
    const uint64_t with_plugin = scripted(1200, &b, nullptr);
    check(with_plugin == GOLDEN, "loaded backend plugin does not touch the sim");
    check(b.has_backend() == !plugin.empty(), "plugin backend attached when given");

    std::printf("  sim-hash = 0x%016llx (bare) 0x%016llx (observer) 0x%016llx (plugin)\n",
                static_cast<unsigned long long>(bare), static_cast<unsigned long long>(observed),
                static_cast<unsigned long long>(with_plugin));
    std::printf("  achievements: %zu/%zu unlocked\n", b.unlocked_count(), b.defined_count());
    std::remove(save.c_str());
    std::printf(failures == 0 ? "game-achievements: PASS\n" : "game-achievements: FAIL\n");
    return failures == 0 ? 0 : 1;
}
