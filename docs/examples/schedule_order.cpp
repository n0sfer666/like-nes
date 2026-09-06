#include "schedule.hpp"

#include <cstdio>
#include <cstring>

#include "platform_args.hpp"

// Comments in docs/examples/ are English on purpose: these files exist to be shown to the reader of
// the documentation, whose source of truth is English (spec #19, decision 1). Everywhere else in the
// tree comments are Russian -- see .context/conventions.md.
//
// What this example claims: the order in which systems run is derived from data (stage plus explicit
// `after` names), never from the order in which they were registered.

// docs:begin(world)
// Systems never see each other. Each one gets the context it was registered with, as `void* user`.
struct World {
    char trace[16] = {};
    unsigned len = 0;

    void note(char c) {
        if (len + 1 < sizeof(trace)) trace[len++] = c;
    }
};
// docs:end(world)

// docs:begin(systems)
static void read_pad(void* user, const framework::Tick&) { static_cast<World*>(user)->note('i'); }
static void aim(void* user, const framework::Tick&) { static_cast<World*>(user)->note('a'); }
static void resolve_hits(void* user, const framework::Tick&) { static_cast<World*>(user)->note('h'); }
static void apply_damage(void* user, const framework::Tick&) { static_cast<World*>(user)->note('d'); }
static void move_camera(void* user, const framework::Tick&) { static_cast<World*>(user)->note('c'); }
// docs:end(systems)

// docs:begin(register)
// `after` names the systems this one must follow. Note that `apply_damage` sorts BEFORE
// `resolve_hits` alphabetically -- the dependency is what puts it second, not the name and not the
// call to add() below.
static const char* const AFTER_RESOLVE[] = {"resolve_hits"};

struct Registration {
    const char* name;
    framework::Stage stage;
    framework::SystemFn fn;
    const char* const* after;
    std::size_t after_count;
};

static const Registration SYSTEMS[] = {
    {"move_camera", framework::Stage::PostSim, move_camera, nullptr, 0},
    {"apply_damage", framework::Stage::Sim, apply_damage, AFTER_RESOLVE, 1},
    {"resolve_hits", framework::Stage::Sim, resolve_hits, nullptr, 0},
    {"aim", framework::Stage::PreSim, aim, nullptr, 0},
    {"read_pad", framework::Stage::Input, read_pad, nullptr, 0},
};

// `forward` walks the table in one direction or the other. Registering the same set in the opposite
// order is the whole point of the example: the built order must not move.
static bool build(framework::Schedule& s, World& w, bool forward) {
    const std::size_t n = sizeof(SYSTEMS) / sizeof(SYSTEMS[0]);
    for (std::size_t k = 0; k < n; ++k) {
        const Registration& r = SYSTEMS[forward ? k : n - 1 - k];
        framework::SystemDesc d;
        d.name = r.name;
        d.stage = r.stage;
        d.fn = r.fn;
        d.user = &w;
        d.after = r.after;
        d.after_count = r.after_count;
        if (!s.add(d)) return false;
    }
    return s.build() == framework::BuildResult::Ok;
}
// docs:end(register)

static void print_order(const framework::Schedule& s) {
    std::printf("order:");
    for (std::size_t i = 0; i < s.size(); ++i) std::printf(" %s", s.name_at(i));
    std::printf("\n");
}

int main(int argc, char** argv) {
    platform::Args args(argc, argv);

    World forward_world;
    framework::Schedule forward_schedule;
    if (!build(forward_schedule, forward_world, true)) {
        std::printf("build failed\n");
        return 1;
    }

    World backward_world;
    framework::Schedule backward_schedule;
    if (!build(backward_schedule, backward_world, false)) {
        std::printf("build failed\n");
        return 1;
    }

    print_order(forward_schedule);

    bool same = forward_schedule.size() == backward_schedule.size();
    for (std::size_t i = 0; same && i < forward_schedule.size(); ++i)
        same = std::strcmp(forward_schedule.name_at(i), backward_schedule.name_at(i)) == 0;
    std::printf("reversed registration gives the same order: %s\n", same ? "yes" : "no");

    // docs:begin(run)
    // One call runs every stage in order. `run_stage` runs a single one -- that is how a fixed-step
    // loop interleaves Sim with rendering.
    framework::Tick tick;
    tick.dt = fix32::from_float(1.0 / 60.0);
    for (tick.index = 0; tick.index < 2; ++tick.index) forward_schedule.run(tick);
    // docs:end(run)

    std::printf("trace after two ticks: %s\n", forward_world.trace);
    return 0;
}
