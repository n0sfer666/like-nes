#pragma once
#include "movers_snapshot.hpp"
#include "rumble_net_settings.hpp"
#include "rumble_play.hpp"

namespace rumble {

namespace br = framework::brawl;

constexpr uint32_t NETWORK_DEPTH = 8;
constexpr uint32_t NETWORK_HISTORY = 64;

struct NetworkSnapshot {
    br::BodyPool pool;
    br::Seats seats;
    framework::ai::AiState ai;
    br::StageCamera camera;
    br::StageSections sections;
    br::StageWaves waves;
    br::LevelProgress progress;
    framework::kinematic::MoversSnapshot movers;
    std::optional<br::StageRopes> ropes;
    uint32_t tick = 0;
    uint32_t wave_tick = 0;
};

struct NetworkEffects {
    uint32_t tick = 0;
    br::HitEvents hits;
    Knockouts knocked;
    br::Seats before;
    br::Seats after;
    br::WaveFacts waves;
    bool checkpoint = false;
};

// This adapter runs the same Sim schedule as a local Play. Output is staged
// by tick so rollback can replace it before the match publishes it.
class NetworkSimulation {
public:
    using Input = PlayerCommand;
    using Snapshot = NetworkSnapshot;

    explicit NetworkSimulation(Play& play, const NetworkSettings& settings);
    void save(Snapshot& out) const;
    void restore(const Snapshot& snapshot);
    void step(const Input* players);

    const NetworkEffects& effects(uint32_t tick) const { return effects_[tick % effects_.size()]; }
    std::optional<uint64_t> hash_at(uint32_t tick) const;
    Play& play() { return play_; }
    uint32_t tick() const { return play_.scene.ticks; }

private:
    struct Hash {
        uint32_t tick = UINT32_MAX;
        uint64_t value = 0;
    };
    void remember_hash();

    Play& play_;
    std::array<NetworkEffects, NETWORK_DEPTH + 1> effects_{};
    std::array<Hash, NETWORK_HISTORY> hashes_{};
};

} // namespace rumble
