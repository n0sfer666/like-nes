# Scenes and levels

English · [Русский](../../ru/guide/scenes-and-levels.md)

A game is more than one level: it has a menu, a level, maybe a pause screen over the level. The
engine keeps them on a **scene stack**, where each scene has its own
[schedule](tick-and-schedule.md), and loads a level from the bundle by its name with one call,
`load_level`, which refuses a broken level with a text instead of crashing. This page covers both,
how long a loaded level lives, and the menu of Neon Rumble, which is built on them.

The headers are `scene_stack.hpp`, `level_load.hpp`, `level_error.hpp` and `scene_level.hpp`, in
the namespace `framework::scene`. They are installed with the SDK
(see [The SDK and your own game](sdk-and-your-game.md)).

## The scene stack

A `Scene` is three fields: a pointer to a built `Schedule`, a `void* user` that is handed to
nothing and lets the game find its own object, and the flag `rollback`, which says whether the
scene runs in a rollback session. A menu does not, a fight does.

`SceneStack` holds up to **4** scenes. `push`, `pop` and `replace` do not change the stack when
they are called. They are queued, up to **8** per tick, and `apply()` performs them in the order
they were submitted. `run(tick)` calls `apply()` and then runs the schedule of the **top** scene
only. The scenes below it stand still, so a pause scene pushed over a level freezes the level and
`pop` resumes it exactly where it stopped. A scene never changes in the middle of a step.

A command is checked when it is submitted, against the depth the stack will have after the queue
is applied, so the caller learns the result at once. A refused command changes nothing:

| `SceneResult` | `scene_reason()` | When |
|---|---|---|
| `BadScene` | `scene without a built schedule` | `push` or `replace` of a scene whose schedule is null or not built |
| `Overflow` | `scene stack is full` | `push` onto 4 scenes, counting the queued commands |
| `Empty` | `scene stack is empty` | `pop` or `replace` with no scene, counting the queued commands |
| `QueueFull` | `too many scene commands in one tick` | a ninth valid command before `apply()` |
| `Rollback` | `scene command from a rollback scene or a resim` | see the next section |

The stack and its queue are not part of the snapshot or the world hash. The level a scene points to
is.

## Commands from a rollback scene

A rollback session re-simulates ticks it has already run. If a system of the fight scene called
`replace` when the level was over, every re-simulation of that tick would submit the command again,
and a command from a predicted tick would still be applied after a correction had undone that tick.
The stack cannot tell these apart, because it is outside the snapshot.

So the stack refuses them. While `run` executes a scene with `rollback = true`, and while `resim`
executes any scene, `push`, `pop` and `replace` return `Rollback` and leave the stack as it was.
`resim(tick)` is the entry point for a rollback session: it runs the top scene's schedule without
`apply()`, because the queue of a confirmed tick must not be applied in the middle of a
re-simulation. An `apply`, `run` or `resim` nested inside a schedule that is running does nothing.

The game submits scene commands from its own code, **after** `run`, by a tick that is confirmed:
"the level is over" or "the load failed" is read from the state, not sent from a system. Neon
Rumble submits them after the step:

<!-- snippet: games/neon-rumble/src/rumble_window.cpp#flow-keys -->
```cpp
LayerStats step_menu(Flow& flow, Hotseat& hotseat, input::GamepadSource& pads, const WindowKeys& keys, Screen& screen,
                     const Credits& credits, const ViewportFit& fit, uint32_t tick) {
    Menu& menu = flow.menu();
    if (keys.hit(WindowKeys::UP)) menu.move(-1);
    if (keys.hit(WindowKeys::DOWN)) menu.move(1);
    const bool enter = keys.hit(WindowKeys::ENTER);
    if (keys.hit(WindowKeys::CREDITS) || (enter && menu.item == MenuItem::Credits)) ++screen.shown;
    pads.poll(hotseat.engine());
    hotseat.engine().drain();
    flow.run(tick);
    if (enter && menu.item != MenuItem::Credits) flow.enter();
    return screen.menu(menu, credits, fit, tick);
}

LayerStats step_level(Flow& flow, Play& play, Hotseat& hotseat, input::GamepadSource& pads, const WindowKeys& keys,
                      Screen& screen, const Credits& credits, const ViewportFit& fit, uint32_t tick) {
    if (keys.hit(WindowKeys::OVERLAY)) screen.overlay = !screen.overlay;
    if (keys.hit(WindowKeys::CREDITS)) ++screen.shown;
    pads.poll(hotseat.engine());
    if (hotseat.tick(play.scene.ticks + 1, play.brawl.players)) {
        flow.run(tick);
        report_step(play.brawl, play.scene.ticks);
    }
    if (keys.hit(WindowKeys::ESCAPE)) flow.leave();
    return screen.level(play, hotseat, credits, fit, tick);
}
```
<!-- /snippet -->

## Loading a level

`load_level(bundle, name, out, error)` finds the level `name` in the bundle and fills `out` with everything a fight needs:

- the `visual` table and its map of `name`, and the collision grid from the `tilemap` table;
- the `objects` table, read into `LevelObjects` by the class dictionary below, and the camera
  bounds from the `bounds` object, or the whole map when there is none;
- the textures the visual layers use;
- the `clips` table and one fighter table per distinct `kind` of the wave spawns, named
  `<kind>_fighter`. Both are opened only if the level has waves.

It returns `true`, or `false` with `error.fault` and `error.text` set. The text is in a fixed buffer
of 192 bytes, and the engine never prints it: showing it is the game's decision. After a refusal
`out` is an empty `Level` again, never a level loaded halfway.

| `LoadFault` | Example of `error.text` |
|---|---|
| `NoTable` | `objects table: missing` |
| `BadTable` | `visual table does not open` |
| `NoLevel` | `no level nope in the visual table` |
| `BadObjects` | `level lvl: checkpoint 'cp9' (id 40) repeats checkpoint order 1 of object id 14` |
| `Texture` | `level lvl: texture <guid in hex>: wrong texture format` |
| `UnknownKind` | `level lvl: spawn a1 w0 kind boss: no table boss_fighter` |
| `BadFighter` | `adler_fighter: no clip adler/jab in the clips table` |
| `Capacity` | `level lvl needs more than 8 fighter kinds` (or 8 textures) |

### The object classes

[Levels in Tiled](tiled-levels.md#objects) describes how objects reach the bundle. `load_level`
gives meaning to these classes and skips every other one, which is left for the game:

| Class | Shape | Properties |
|---|---|---|
| `spawn` | point | without `wave`, a player's spawn. With `wave` (0 or more): `arena`, the id of an arena of the level; `kind`, not empty; optional `players` (at least 1) |
| `checkpoint` | point | `order`, 0 to 32767, unique in the level |
| `arena` | rectangle | `id`, 0 to 32767, unique in the level |
| `section` | rectangle | `mode`, `belt` or `platform` |
| `autoscroll` | rectangle | `axis`, `x` or `y`, and `speed` |
| `depth_band`, `wall`, `kill` | rectangle | none |
| `conveyor` | rectangle | `speed` |
| `path` | polygon | `speed`, `loop`; at most 16 vertices |
| `rope` | point | `length` |
| `bounds` | rectangle | none; at most one per level |

A missing property, a property of the wrong type or out of its range, an object of the wrong
shape and a link to an arena that does not exist are a `BadObjects` refusal that names the object
and its id. The range of `checkpoint.order` is not arbitrary: the level's progress stores the
taken order in 16 bits, with −1 for "none taken", so an order above 32767 would never be taken. Two
checkpoints with the same order are refused because the progress stores only the order, and the
respawn finds the first checkpoint with it: a player who took the second would come back at the
first.

## How long a Level lives

A `Level` copies nothing out of the bundle. Its name, the `kind` of each fighter, the pixels of the
textures and the rows of every table are views into the mapped bundle, so a `Level` must not
outlive the mapping it was loaded from. The bundle itself is kept inside as `level.bundle`, and
`load_level` takes it **by value**: `load_level(level.bundle, "level2", level, error)` reloads
into the same `Level` safely, although the first thing `load_level` does is to clear `out`.

A `Level` does not move once a fight is opened on it: the fight keeps pointers to it, and the
schedule of a scene keeps pointers to its systems' data. Neon Rumble therefore keeps the level,
the fighters, the fight and the scene together in one heap object, `Play`, and loads it only on
entry:

<!-- snippet: games/neon-rumble/src/rumble_play.cpp#level-enter -->
```cpp
std::unique_ptr<Play> enter_level(const asset::BundleView& bundle, const char* name, bool waves, std::string& error) {
    auto play = std::make_unique<Play>();
    framework::scene::LoadError load;
    if (!framework::scene::load_level(bundle, name, play->level, load)) {
        error = load.text;
        return nullptr;
    }
    for (uint32_t i = 0; i < FIGHTERS; ++i)
        if (!play->fighters[i].open(play->level, ROSTER[i].fighter)) {
            error = std::string("fighter ") + ROSTER[i].fighter + " does not open";
            return nullptr;
        }
    if (!play->brawl.open(play->level, play->fighters, waves)) {
        error = std::string("brawl of level ") + name + " does not open";
        return nullptr;
    }
    if (!play->scene.init(play->brawl)) {
        error = "schedule of the level does not build";
        return nullptr;
    }
    return play;
}
```
<!-- /snippet -->

The scene's `Schedule` and `user` must live while the scene is on the stack **or in the queue**.
Until `apply()` a queued `replace` needs both: the stack still holds the old scene, and the queue
already holds the new one. Neon Rumble releases its
level in `apply()`, once the menu is on top again:

<!-- snippet: games/neon-rumble/src/rumble_flow.cpp#flow-apply -->
```cpp
bool Flow::open() {
    const sc::SceneResult r = stack_.push(menu_.stack_scene());
    if (r != sc::SceneResult::Ok) {
        std::fprintf(stderr, "neon-rumble: menu scene: %s\n", sc::scene_reason(r));
        return false;
    }
    return apply();
}

bool Flow::apply() {
    stack_.apply();
    const void* top = stack_.top() == nullptr ? nullptr : stack_.top()->user;
    if (top == bound_) return false;
    bound_ = top;
    if (top == &menu_) play_.reset();
    return true;
}
```
<!-- /snippet -->

## The menu of Neon Rumble

The window always opens on the menu. It has three items, drawn with the credits font:
`level level1`, `wave level1` and `credits`. **↑** and **↓** select, **Enter** enters. Entering a
level calls `enter_level` above and then `replace` with the level's scene, so the next frame starts
in the level. **Esc** in a level replaces it with the menu, and the next entry loads the level again
from the bundle.

When the load is refused, the game stays in the menu, shows the text of the refusal above the
items and prints it to stdout as a `menu:` line:

<!-- snippet: games/neon-rumble/src/rumble_flow.cpp#flow-leave -->
```cpp
void Flow::leave() {
    const sc::SceneResult r = stack_.replace(menu_.stack_scene());
    if (r != sc::SceneResult::Ok) std::fprintf(stderr, "neon-rumble: back to the menu: %s\n", sc::scene_reason(r));
}

void Flow::refuse(const std::string& error) {
    menu_.error = error;
    std::printf("neon-rumble: menu: %s\n", error.c_str());
    std::fflush(stdout);
}
```
<!-- /snippet -->

`--level <name>` sets the level of the menu items, and `--wave` selects the `wave` item. A headless
run has no menu: it enters the selected item at once. A refused load prints the same line among the
others and ends the run with exit code 0, which is how CI checks the way back to the menu on three
OSes:

```sh
neon_rumble --headless --level nope --frames 1
```

```text
neon-rumble: menu: no level nope in the visual table
```
