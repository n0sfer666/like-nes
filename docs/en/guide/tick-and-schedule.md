# The tick and the schedule

English · [Русский](../../ru/guide/tick-and-schedule.md)

A frame of simulation is a **tick**: a fixed step of time in which every registered system runs
exactly once, in an order the engine derives rather than remembers. This page is about that order —
what decides it, and why it is not the order in which you registered anything.

Everything shown below is taken from [`docs/examples/schedule_order.cpp`](../../examples/schedule_order.cpp),
which is built and run by CI on all three operating systems. The output at the end of the page is
the output that program actually prints.

## Stages are the coarse grid

There are five stages, and games do not add their own:

| Stage | What belongs there |
|---|---|
| `Input` | raw device state turned into player actions |
| `PreSim` | intent derived from actions: desired velocity, jump and fire requests |
| `Sim` | physics and world rules — this is what the simulation hash covers |
| `PostSim` | things derived from the new state: camera, animation, sound events |
| `Present` | preparation for drawing; not part of the simulation hash |

A system in an earlier stage always runs before a system in a later one. That is the whole of the
guarantee stages give you; inside a stage, order comes from dependencies.

## A system is a function plus a context

Systems do not see each other. Each one receives the pointer it was registered with:

<!-- snippet: docs/examples/schedule_order.cpp#world -->
```cpp
// Systems never see each other. Each one gets the context it was registered with, as `void* user`.
struct World {
    char trace[16] = {};
    unsigned len = 0;

    void note(char c) {
        if (len + 1 < sizeof(trace)) trace[len++] = c;
    }
};
```
<!-- /snippet -->

<!-- snippet: docs/examples/schedule_order.cpp#systems -->
```cpp
static void read_pad(void* user, const framework::Tick&) { static_cast<World*>(user)->note('i'); }
static void aim(void* user, const framework::Tick&) { static_cast<World*>(user)->note('a'); }
static void resolve_hits(void* user, const framework::Tick&) { static_cast<World*>(user)->note('h'); }
static void apply_damage(void* user, const framework::Tick&) { static_cast<World*>(user)->note('d'); }
static void move_camera(void* user, const framework::Tick&) { static_cast<World*>(user)->note('c'); }
```
<!-- /snippet -->

## Registration order is not execution order

`after` names the systems a system must follow. Ties — systems in the same stage with nothing
between them — are broken by name, so that a set of systems has exactly one valid order:

<!-- snippet: docs/examples/schedule_order.cpp#register -->
```cpp
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
```
<!-- /snippet -->

This matters more than it looks. Modules register from different translation units, and the order
of static initialization between translation units is not defined by the standard — it follows the
order of objects on the linker command line. A schedule that honoured registration order would give
you a different simulation hash when someone reorders two libraries in CMake, and the only way to
find that would be to diff two builds.

`build()` reports what went wrong by name: an unknown dependency, a dependency that lives in a later
stage (unsatisfiable by construction), a duplicate name, a cycle. `error_system()` names the system
the analysis stopped on.

## Running it

<!-- snippet: docs/examples/schedule_order.cpp#run -->
```cpp
// One call runs every stage in order. `run_stage` runs a single one -- that is how a fixed-step
// loop interleaves Sim with rendering.
framework::Tick tick;
tick.dt = fix32::from_float(1.0 / 60.0);
for (tick.index = 0; tick.index < 2; ++tick.index) forward_schedule.run(tick);
```
<!-- /snippet -->

`dt` is [fixed point](determinism.md), not a float, because it goes into the simulation hash.

The whole program prints:

<!-- snippet: docs/examples/schedule_order.out -->
```text
order: read_pad aim resolve_hits apply_damage move_camera
reversed registration gives the same order: yes
trace after two ticks: iahdciahdc
```
<!-- /snippet -->

`apply_damage` sorts before `resolve_hits` alphabetically and still runs after it; reversing every
call to `add()` changes nothing. That is the property the schedule exists for.
