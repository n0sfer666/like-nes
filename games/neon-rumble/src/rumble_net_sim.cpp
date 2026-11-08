#include "rumble_net_sim.hpp"

#include "hash_mix.hpp"

namespace rumble {

NetworkSimulation::NetworkSimulation(Play& play, const NetworkSettings& settings) : play_(play) {
    auto& brawl = play.brawl;
    brawl.hit_rules.friendly_fire = settings.friendly_fire;
    brawl.wave.ai.rng.state = settings.seed;
    switch (settings.difficulty) {
        case Difficulty::Easy: brawl.wave.token_budget = {1, 1, 1}; break;
        case Difficulty::Normal: break;
        case Difficulty::Hard: brawl.wave.token_budget = {1, 2, 3}; break;
    }
    remember_hash();
}

void NetworkSimulation::save(Snapshot& out) const {
    const auto& brawl = play_.brawl;
    const auto& wave = brawl.wave;
    out.pool = brawl.pool;
    out.seats = brawl.seats;
    out.ai = wave.ai;
    out.camera = wave.camera;
    out.sections = wave.sections;
    out.waves = wave.waves;
    out.progress = wave.progress;
    if (wave.platform.movers) framework::kinematic::save_movers(*wave.platform.movers, out.movers);
    out.ropes = wave.platform.ropes;
    out.tick = play_.scene.ticks;
    out.wave_tick = wave.tick;
}

void NetworkSimulation::restore(const Snapshot& snapshot) {
    auto& brawl = play_.brawl;
    auto& wave = brawl.wave;
    brawl.pool = snapshot.pool;
    brawl.seats = snapshot.seats;
    wave.ai = snapshot.ai;
    wave.camera = snapshot.camera;
    wave.sections = snapshot.sections;
    wave.waves = snapshot.waves;
    wave.progress = snapshot.progress;
    if (wave.platform.movers) framework::kinematic::restore_movers(snapshot.movers, *wave.platform.movers);
    wave.platform.ropes = snapshot.ropes;
    play_.scene.ticks = snapshot.tick;
    wave.tick = snapshot.wave_tick;
}

void NetworkSimulation::step(const Input* players) {
    auto& brawl = play_.brawl;
    const uint32_t next = tick() + 1;
    auto& effect = effects_[next % effects_.size()];
    effect.before = brawl.seats;
    for (uint32_t i = 0; i < br::SEATS; ++i) brawl.players[i] = players[i];
    play_.scene.schedule.run_stage(framework::Stage::Sim, frame_tick(tick()));
    effect.tick = tick();
    effect.hits = brawl.events;
    effect.knocked = brawl.knocked;
    effect.after = brawl.seats;
    effect.waves = brawl.wave.facts;
    effect.checkpoint = brawl.wave.took;
    remember_hash();
}

void NetworkSimulation::remember_hash() {
    uint64_t hash = play_.brawl.hash();
    framework::physics::mix(hash, tick());
    history_[tick() % history_.size()] = {tick(), hash, play_.brawl.players};
}

std::optional<uint64_t> NetworkSimulation::hash_at(uint32_t at) const {
    const auto* frame = replay_at(at);
    return frame ? std::optional<uint64_t>{frame->hash} : std::nullopt;
}

const NetworkReplayFrame* NetworkSimulation::replay_at(uint32_t at) const {
    const auto& entry = history_[at % history_.size()];
    return at <= tick() && entry.tick == at ? &entry : nullptr;
}

} // namespace rumble
