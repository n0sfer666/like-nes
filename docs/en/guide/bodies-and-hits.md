# Bodies and hits

English · [Русский](../../ru/guide/bodies-and-hits.md)

A brawler is a crowd of bodies on a street with depth: players, enemies, thrown knives, crates. The
engine keeps them in one pool, moves them with the same rules every tick and decides which punch
lands with plain fixed-point geometry, so two machines that fed the same inputs hold the same world
bit for bit (see [Determinism and fixed-point arithmetic](determinism.md)). This page covers the
body, the pool, the tick of a fight, the rules of a hit and the `.fighter` file that describes a
fighter.

The headers are `depth_body.hpp`, `brawl_body.hpp`, `body_pool.hpp`, `brawl_input.hpp`,
`brawl_step.hpp`, `hit_events.hpp`, `reaction.hpp`, `fighter_read.hpp` and `archetype.hpp`, in the
namespace `framework::brawl`. They are installed with the SDK (see [The SDK and your own
game](sdk-and-your-game.md)).

## A body on a street with depth

`DepthBody` is a position and a velocity on three axes, all `fix32` in pixels: `x` runs along the
level, `z` is the depth of the street, `y` is the height above the floor, up is positive. The screen
sees `(x, z - y)`, and only the renderer computes it: a jump changes `y`, walking up the street
changes `z`, and the simulation never mixes the two.

`Body` adds the rest of a fighter: its `EntId`, `facing`, `team` and `owner`, `hp`, the archetype
`kind`, the clip and the move it plays with their `elapsed` ticks, `hitstop`, `immune`, the reaction
with its ticks, the list of bodies its current swing has struck, the chain step, the queued attack,
the run and the grip. Team `0` is the players.

`BodyPool` holds up to **26** bodies (`POOL_CAPACITY`: 2 players, 8 enemies, 16 projectiles and
props). `spawn` hands out an `EntId` whose `seq` only grows, `despawn` removes a body, `find` looks
one up. A freed `seq` is never reused, so an old id cannot point at a new body; the counter
`next_seq` is part of the snapshot, and the pool is walked and hashed in `seq` order.

## The tick of a fight

A body is driven by a `Command`: a `BrawlInput` and the attack the game chose for it. `BrawlInput`
is a `MoveInput` (the stick on two axes), the bits `button::ATTACK`, `JUMP`, `GRAB`, `BLOCK`,
`DODGE` and `RUN` (0 to 5) and the flag `present`; an empty `BrawlInput{}` means nobody is holding
the controls. `BrawlWorld` is what every body reads and nobody writes: the archetypes, the floor,
the `HitRules`, and for a level the level objects and the platform world (see [Camera and
sections](camera-and-sections.md)).

`step_brawl(pool, commands, world, events)` is `step_bodies` and then `resolve_hits`: every body
moves first, then hits are collected against the moved bodies and applied all at once, so the order
of the pool never decides who hit first. The full order of systems in a tick, which the level step
of a brawler follows:

1. input and the `present` flag of the seats ([Hotseat](hotseat.md));
2. AI fills the commands of the enemies;
3. reactions and states advance;
4. movement;
5. collision and section changes;
6. the camera;
7. hits, collected and then applied;
8. clips: `elapsed + 1`, `hitstop - 1`;
9. waves, checkpoints and deaths.

Neon Rumble builds the commands of its two players in the game and hands the whole pool to the step.
The strike of a player is picked from its buttons; enemies already have theirs from the AI:

<!-- snippet: games/neon-rumble/src/rumble_brawl.cpp#brawl-step -->
```cpp
for (uint32_t i = 0; i < pool.count; ++i) {
    const br::Body& b = pool.bodies[i];
    if (b.team == 0 && b.kind < br::SEATS)
        commands[i].strike = strike_of(players[b.kind].attack, kinds.moves[b.kind], b);
}
const br::BrawlWorld world{kinds.types, &floor, br::HitRules{}};
if (!wave.on) {
    br::step_brawl(pool, std::span<const br::Command>(commands.data(), pool.count), world, events);
    return;
}
const uint32_t before = pool.count;
wave.step(pool, seats, world, commands, events);
remember(*this, before);
```
<!-- /snippet -->

## When a hit lands

Every frame of a clip carries up to four hit boxes and hurt boxes (see [Animations in
Aseprite](aseprite-animations.md#hit-boxes-hurt-boxes-and-the-pivot)). A box faces where the body
faces: its `x` is `x + facing * rx`, and its height runs from `y - ry - h` to `y - ry`. A hit box
meets a hurt box when they overlap on `x` and on the height, both as half-open ranges, **and** the
depths are close: `|z_attacker - z_target| < depth_attacker + depth_target`, strictly. Boxes that
only touch at an edge do not hit.

A hit is refused before the geometry when the target is the attacker itself, a body the attacker
owns, or a body of the same team while `HitRules::friendly_fire` is `false`. A thrown body and a
projectile take the team and the owner of whoever threw them. One swing strikes a body once: the
struck ids are kept for the length of the move (up to **16**). A body with `immune > 0` is not hit
(the game sets it, for example on a respawn; `immune = N` keeps the body closed for the next `N - 1`
collections). Only a body in the reaction `None` or `Hurt` can be hit; `Fall`, `Down`, `Getup`,
`Dodge` and `Thrown` cannot, and `Block` is covered below.

All the hits of a tick are applied together. Damage adds up and `hp` stops at 0. `hitstop` is the
largest of the events, not the sum. The knockback and the hitstun come from the event with the
largest hitstun; on a tie the attacker with the smaller `seq` wins, then the first box. The target
gets `vx = facing * knock_x`, `vz = 0` and `vy = knock_y` when it is above 0 or the target is in the
air. Up to **64** events are kept per tick (`MAX_HIT_EVENTS`); `HitEvents::dropped` counts the rest.

A `launch` sends the target into `Fall` until it lands, then `Down` for `down` ticks, then `Getup`
for `getup` ticks. **Block** (`BLOCK` held, on the ground) stops a `light` or `heavy` hit from the
front: no damage, no reaction, both sides get the hitstop, and the target slides back at half the
knockback for the hitstun. A hit from behind and a `launch` go through. **Dodge** makes the body
untouchable for `dodge` ticks and moves it at `facing * run_x`. **Run** starts on a double tap
within `run_tap` ticks and moves at `run_x`; a move marked `slide` rolls on at `facing * run_x`.

A **chain** is the list of moves the attack button walks through. A press is buffered for `buffer`
ticks; the next step starts when the buffer holds the press, the current clip has reached its
`cancel` event and the current move has landed. Every step but the last needs a `cancel` event in
its clip. A **grab** checks its first hit box against the `push` box of the target (the lower half
of its first hurt box on frame 0), goes through a block and puts the target into `Thrown` at the
anchor of the thrower; the thrown body flies at the grab's `knock_x`/`knock_y` and hits others with
the `throw` move. Two fighters grabbing each other in the same tick both let go. A move with
`hits_down yes` hits only a body that is `Down`.

## The .fighter file

A fighter is a text file of the same `|` grammar as the asset manifest (see [Levels in
Tiled](tiled-levels.md)). The manifest puts it in the bundle as a table named `<name>_fighter`, for
example `fighter | adler_fighter | fighters/adler.fighter`. Its clips are found by name,
`<sheet>/<tag>`, in the `clips` sheet of the level.

The head of the file:

| key | range | meaning |
|---|---|---|
| `sheet` | name | the sprite sheet, equal to the fighter name |
| `speed_x`, `speed_z`, `run_x`, `jump_vy` | 0..16 | walk along and across the street, run, jump, px per tick |
| `gravity` | >0..16 | px per tick per tick |
| `depth` | >0..64 | half the depth of the body for hits |
| `hp` | 1..9999 | health at spawn |
| `down`, `getup`, `buffer`, `run_tap`, `dodge` | 1..120 | ticks |
| `chain` | 1..8 moves | the attack chain; the last move is the finisher |

Then one `move | <name> | hit0` .. `hit3` line per hit box of a move:

| key | range | meaning |
|---|---|---|
| `clip` | name | the clip of the move, the move name when left out |
| `type` | `light`, `heavy`, `launch`, `grab`, `throw` | how the target reacts |
| `damage` | 0..999 | |
| `depth` | >0..64 | the depth reach of this box |
| `hitstop`, `hitstun` | 0..120 | ticks |
| `knock_x`, `knock_y` | -16..16 | `knock_x > 0` pushes away along the attacker's facing, `knock_y > 0` is up |
| `hits_down`, `slide` | `yes`, `no` | |

The `adler` of Neon Rumble walks at 2 and 1, runs at 3, jumps at 6 with gravity 0.5, has depth 6,
100 hp and the chain `jab jab kick`; its `jab` is `light`, damage 6, depth 8, hitstop 4, hitstun 10,
knock 1 and 0 — see
[`games/neon-rumble/fighters/adler.fighter`](../../../games/neon-rumble/fighters/adler.fighter):

```text
move | jab | hit0
type      | light
damage    | 6
depth     | 8
```

`FighterTable::open` reads the baked table, `make_archetype(table, clips, out, error)` turns it into
an `Archetype` and checks that the sheet has the clips every fighter needs: `idle`, `walk`, `jump`,
`hurt`, `fall`, `down`, `getup`, `block`, `dodge`, `throw` and `thrown`. A broken file is refused
with a line of text, for example `unknown key 'speeed_x'`, `… is outside the range the engine
accepts`, `type must be light, heavy, launch, grab or throw`, `chain must name 1 to 8 moves` or
`move 'jab' plays two clips`. Neon Rumble opens each fighter like this:

<!-- snippet: games/neon-rumble/src/rumble_kinds.cpp#kind-open -->
```cpp
bool open_kind(const Level& level, const Fighter& f, br::Archetype& out, int32_t& hp) {
    const std::string table = std::string(f.name) + "_fighter";
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!read_table(level, table.c_str(), data, size)) return false;
    br::FighterTable t;
    if (!t.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: %s table: does not open\n", table.c_str());
        return false;
    }
    if (std::string(t.sheet()) != f.name) {
        std::fprintf(stderr, "neon-rumble: %s: sheet %s is not %s\n", table.c_str(), t.sheet(), f.name);
        return false;
    }
    std::string error;
    if (!br::make_archetype(t, f.clips, out, error)) {
        std::fprintf(stderr, "neon-rumble: %s: %s\n", table.c_str(), error.c_str());
        return false;
    }
    hp = static_cast<int32_t>(t.hp());
    return true;
}
```
<!-- /snippet -->
