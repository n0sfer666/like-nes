#include <cinttypes>
#include <cstdio>

#include "framework_brawl_check.hpp"
#include "framework_brawl_rope_fixture.hpp"

// Гейт энергии троса (спека #25, В8в): без ввода энергия маятника не растёт 10 000 тиков — на
// голом шаге по длинам и размахам от предела до предела и с седоком в полном тике драки. Сломанная
// фикстура — явный Эйлер: тот же маятник с дугой от прошлой скорости раскачивает себя сам. Снапшот
// тросов — копия `StageRopes`; забытое поле расходится с честным пересчётом.
namespace {

constexpr uint64_t GOLDEN = 0xeb3bfbcf270b2683;

using namespace framework::brawl;
using namespace framework::brawl::test;
using framework::Vec2;

constexpr uint32_t TICKS = 10000;
constexpr int32_t LENGTHS[] = {16, LENGTH, 256};
constexpr int32_t DEGREES[] = {5, 45, 89};

using Swinger = void (*)(RopeState&, fix32, fix32);

void semi_implicit(RopeState& r, fix32 length, fix32 g) { swing_rope(r, length, g, fix32{}); }

void explicit_euler(RopeState& r, fix32 length, fix32 g) {
    const fix32 pull = g * framework::sin_turns(rope_turns(r.arc, length));
    r.arc = r.arc + r.speed;
    r.speed = r.speed - pull;
    r.speed = r.speed - fix32::from_raw(r.speed.raw / ROPE_DAMPING);
    const fix32 cap = rope_cap(length);
    if (!(cap < framework::abs_fix(r.arc))) return;
    r.arc = framework::clamp_fix(r.arc, -cap, cap);
    r.speed = fix32{};
}

bool settles(Swinger swing, int32_t length, int32_t degrees) {
    const fix32 l = at(length);
    const fix32 g = Swing().g();
    RopeState r;
    r.arc = deflection(l, degrees);
    EnergyGate gate(rope_energy(r, l, g));
    for (uint32_t t = 0; t < TICKS; ++t) {
        swing(r, l, g);
        gate.add(rope_energy(r, l, g));
    }
    return gate.settles();
}

void bare_swing() {
    for (const int32_t l : LENGTHS)
        for (const int32_t d : DEGREES) {
            const bool honest = settles(semi_implicit, l, d);
            const bool broken = settles(explicit_euler, l, d);
            if (!honest || broken) std::printf("    length %d, %d degrees: honest %d, explicit %d\n", l, d, honest, broken);
            check(honest, "the rope gains energy without input");
            check(!broken, "explicit Euler passes the energy gate");
        }
}

void rider_swing() {
    Swing s;
    const EntId id = s.fighter(500);
    s.ropes.at[0].arc = deflection(at(LENGTH), 45);
    s.hang_hands(id, s.end());
    s.step({grab()});
    check(s.ropes.at[0].rider == id, "the fighter does not take the rope for the energy run");
    EnergyGate gate(rope_energy(s.ropes.at[0], at(LENGTH), s.g()));
    bool follows = true;
    for (uint32_t t = 0; t < TICKS; ++t) {
        s.step({});
        gate.add(rope_energy(s.ropes.at[0], at(LENGTH), s.g()));
        const Vec2 hands = s.hands(id);
        follows = follows && hands.x == s.end().x && hands.y == s.end().y;
    }
    check(gate.settles(), "the rope with a rider gains energy without input");
    check(s.ropes.at[0].rider == id && follows, "the rider leaves the rope end without input");
}

// Седок качается, отпускает трос прыжком и всё остывание держит руки у конца с зажатым захватом:
// снимок на первом тике остывания, когда в тросе живы все четыре поля, и трос берётся снова.
struct Script {
    Swing s;
    EntId id;
    uint32_t tick = 0;

    Script() : id(s.fighter(500)) {
        s.ropes.at[0].arc = deflection(at(LENGTH), 30);
        s.hang_hands(id, s.end());
        s.step({grab()});
    }

    void step(uint32_t ticks) {
        for (uint32_t t = 0; t < ticks; ++t, ++tick) {
            if (tick == 30) {
                s.step({leap()});
                continue;
            }
            if (s.ropes.at[0].rider == EntId{}) s.hang_hands(id, s.end());
            s.step({grab(tick < 30 ? 1 : -1)});
        }
    }
};

enum class Forget : uint8_t { Nothing, Arc, Speed, Rider, Cooldown };

uint64_t resim(Forget forget) {
    Script run;
    run.step(32);
    const BodyPool pool = run.s.w.s.pool;
    const StageSections sections = run.s.w.s.sections;
    const StageCamera camera = run.s.w.s.camera;
    const StageRopes saved = run.s.ropes;
    const uint32_t tick = run.tick;
    run.step(70);
    const uint64_t straight = run.s.hash();
    run.s.w.s.pool = pool;
    run.s.w.s.sections = sections;
    run.s.w.s.camera = camera;
    run.tick = tick;
    RopeState& r = run.s.ropes.at[0];
    const RopeState was = saved.at[0];
    if (forget != Forget::Arc) r.arc = was.arc;
    if (forget != Forget::Speed) r.speed = was.speed;
    if (forget != Forget::Rider) r.rider = was.rider;
    if (forget != Forget::Cooldown) r.cooldown = was.cooldown;
    run.step(70);
    return straight ^ run.s.hash();
}

void snapshot() {
    Script run;
    run.step(32);
    check(run.s.ropes.at[0].rider == EntId{} && run.s.ropes.at[0].cooldown == ROPE_COOLDOWN - 1,
          "the script does not cut the snapshot in the cooldown");
    run.step(70);
    std::printf("  rope ride hash = 0x%016" PRIx64 "\n", run.s.hash());
    check(run.s.hash() == GOLDEN, "golden of the rope ride");
    check(run.s.ropes.at[0].rider == run.id, "the script does not retake the rope after the cooldown");
    check(resim(Forget::Nothing) == 0, "a restored snapshot does not replay the rope");
    check(resim(Forget::Arc) != 0, "a snapshot without the rope arc replays the rope");
    check(resim(Forget::Speed) != 0, "a snapshot without the rope speed replays the rope");
    check(resim(Forget::Rider) != 0, "a snapshot without the rope rider replays the rope");
    check(resim(Forget::Cooldown) != 0, "a snapshot without the rope cooldown replays the rope");
    StageRopes ropes = run.s.ropes;
    uint64_t last = ropes_hash(ropes);
    bool sees = true;
    const auto changed = [&] {
        const uint64_t h = ropes_hash(ropes);
        sees = sees && !(h == last);
        last = h;
    };
    ropes.at[0].arc = ropes.at[0].arc + fix32::from_raw(1);
    changed();
    ropes.at[0].speed = ropes.at[0].speed + fix32::from_raw(1);
    changed();
    ropes.at[0].rider = EntId{77};
    changed();
    ropes.at[0].cooldown = 3;
    changed();
    check(sees, "the rope hash does not see a rope field");
}

} // namespace

int main() {
    bare_swing();
    rider_swing();
    snapshot();
    return verdict("framework-brawl-rope");
}
