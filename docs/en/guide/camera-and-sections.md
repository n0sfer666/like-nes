# Camera and sections

English · [Русский](../../ru/guide/camera-and-sections.md)

In a brawler the camera is a rule of the game, not a decoration: it locks the players in an arena
until the wave is beaten, keeps them inside the view and pushes them forward in an autoscroll. So
the camera lives in the simulation, in the snapshot and the world hash, and the level decides with
it where a fight starts. A level can also switch from the street with depth to a platformer and
back. This page covers the camera, the sections, the arenas with their waves and the checkpoints.

The objects these rules read — `arena`, `section`, `autoscroll`, `checkpoint`, `bounds` and `spawn`
— are laid out in Tiled and described in [Scenes and
levels](scenes-and-levels.md#the-object-classes); conveyors, lifts, ropes and projectiles are
covered there as well. The headers are `stage_camera.hpp`, `stage_step.hpp`, `stage_sections.hpp`,
`platform_world.hpp`, `stage_waves.hpp`, `wave_step.hpp`, `wave_facts.hpp`, `next_arena.hpp`,
`level_progress.hpp` and `checkpoint_step.hpp`, in the namespace `framework::brawl`. They are
installed with the SDK (see [The SDK and your own game](sdk-and-your-game.md)).

## The level step

`step_stage` is [`step_brawl`](bodies-and-hits.md#the-tick-of-a-fight) for a level. In one call it
moves the lifts and conveyors, the ropes, lets the bodies spawned this tick enter their section,
moves every body, changes sections, steps the camera and resolves the hits. Waves, checkpoints and
deaths come after it, as separate calls of the game. Neon Rumble runs a level like this; the bodies
of a new wave are spawned by the game from the facts `step_waves` returns:

<!-- snippet: games/neon-rumble/src/rumble_wave.cpp#wave-step -->
```cpp
void Wave::step(br::BodyPool& pool, const br::Seats& seats, br::BrawlWorld world, std::span<br::Command> commands,
                br::HitEvents& events) {
    world.level = &level;
    world.platform = platform.world ? &*platform.world : nullptr;
    const br::DepthFloor reached = platform.reach(*world.floor, level, waves.cleared);
    world.floor = &reached;
    br::note_living(pool, living);
    ai.tokens.capacity = TOKENS_BY_PLAYERS[players(seats)];
    ai::step_ai(ai, pool, world, tick, foe_brain(), commands);
    throw_knives(ai.brains, pool, sections, world);
    ++tick;
    br::step_stage(pool, std::span<const br::Command>(commands.data(), pool.count), world, camera, seats, sections,
                   events, platform.movers.get(), platform.ropes ? &*platform.ropes : nullptr);
    br::step_deaths(pool, living, deaths);
    took = br::step_checkpoints(progress, pool, seats, level);
    was_arena = camera.arena;
    was_wave = waves.wave;
    br::step_waves(waves, camera, pool, world.kinds, seats, level, br::WaveRules{WAVE_LULL}, facts);
    spawned_count = 0;
    for (uint32_t i = 0; i < facts.count; ++i) {
        const br::EntId id = pool.spawn(foes[facts.starts[i]]);
        if (id == br::EntId{}) {
            ++facts.dropped;
            continue;
        }
        if (!ai::add_brain(ai.brains, id, foe_start(foes[facts.starts[i]].kind))) {
            pool.despawn(id);
            ++facts.dropped;
            continue;
        }
        spawned[spawned_count++] = id;
    }
}
```
<!-- /snippet -->

## The camera

`StageCamera` holds the `center` of the view, the locked `arena` (`NO_ARENA`, -1, when none) and the
state of the autoscroll. Screen shake is not here: it is a decoration of the renderer's
`graphics::Camera` and never touches the simulation. `step_camera` runs inside `step_stage`:

1. no player is alive — the camera stays where it is;
2. `center.x` is the middle of the span of the living players along `x`;
3. in a `belt` section `center.y` does not follow; in a `platform` section it follows the middle of
   the players on screen with a dead zone of `PlatformWorld::dead_y` pixels (24 in Neon Rumble);
4. an autoscroll moves the center (see below);
5. the center is clamped to the locked arena, then to the `bounds` of the level;
6. the **leash** puts every living player back inside the view zone (**384 × 216**): its velocity
   along the clamped axis becomes 0.

The game sets the center itself only when a level opens: Neon Rumble puts it at player 1, clamped,
and sets `StageWaves::seen` to the same `x`.

**Autoscroll.** While the center is inside an `autoscroll` rectangle, the camera rides it once at
its `speed` along its `axis`; it does not ride it again when the players come back. When the ride
ends the camera returns to the players at `max(|speed|, gap / 8)` per tick, so it neither jumps nor
drags. A player that the leash of an `x` autoscroll pushes into a wall gets `Body::crushed`; what
that means — damage, a respawn — is the game's decision.

## Sections

A `section` rectangle sets the mode of the level along `x`: the first section with `x0 ≤ x < x1`
wins, and outside every section the level is a `belt`. In a `platform` section a body is driven by
the engine's character controller, the one a platformer uses; `PlatformWorld` holds its collision
scene, the hull, the move profile and the tick length. Neon Rumble uses a hull of 8 × 32 half-size,
60 ticks per second and the default profile. The state of every body in a platform section — its
`EntId` and its controller — sits in `StageSections`, which has its own hash.

- A body changes section only on the ground; in the air the change waits for the landing.
- When the point it would land on in the new section is taken by a tile or another body, it does not
  enter: its `x` is held at the border.
- A body spawned straight into a platform section on a taken point is moved to the nearest free
  point inside the section, a body width at a time, right first and then left (see
  [Hotseat](hotseat.md#joining-a-running-level)).
- Hits land only between bodies in the same mode.

## Arenas and waves

`StageWaves` holds the last cleared arena, the current wave, the lull and the `seen` position of the
camera. `step_waves(waves, camera, pool, kinds, seats, level, rules, facts)` runs after the level
step:

- With no arena locked, it takes the **next arena**: the one with the smallest `id` above the last
  cleared. It is locked when the center has reached or passed the point it will be clamped to there,
  measured from `seen` to the new center, so a fast camera cannot skip it. `facts.locked` is set and
  the first wave starts.
- A wave starts with every `spawn` of that arena and wave whose `players` is at most the number of
  present seats. `facts.starts` lists them; the game spawns the bodies (up to the pool, the rest in
  `facts.dropped`). The number of players is read when a wave starts, so a player who joins or
  leaves changes the next wave, not this one.
- A wave is cleared when no fighter of a team other than 0 is left (`facts.cleared`). The next wave
  comes after `WaveRules::lull` ticks (120 in Neon Rumble); after the last one the arena is released
  (`facts.released`), the camera is free and `cleared` moves on.

`next_arena(level, cleared)` is that same rule as a function, and returns `nullptr` when every arena
is cleared. A game that needs to know whether a fight is still ahead calls it instead of keeping its
own copy. Neon Rumble widens the street up to the end of the platform section only when no arena is
left:

<!-- snippet: games/neon-rumble/src/rumble_platform.cpp#platform-reach -->
```cpp
br::DepthFloor Platform::reach(const br::DepthFloor& floor, const sc::LevelObjects& level, int16_t cleared) const {
    br::DepthFloor out = floor;
    if (!world) return out;
    if (br::next_arena(level, cleared) != nullptr) return out;
    for (const sc::Section& s : level.sections)
        if (s.mode == sc::SectionMode::Platform) out.band.x1 = framework::max_fix(out.band.x1, s.area.rect.x1);
    return out;
}
```
<!-- /snippet -->

## Checkpoints

`LevelProgress` holds the `order` of the last checkpoint taken. `step_checkpoints` takes a
checkpoint when a living player stands on the ground at or past its `x`, and only one with a larger
`order` than the current. `find_checkpoint(level, order)` returns it, for the game's respawn and for
a player who joins later.

In `level1` of Neon Rumble the street runs from 104 to 1560 with arena 0 at 520..968 and arena 1 at
1176..1560; the platform section with a conveyor runs from 1568 to 2200. See [Levels in
Tiled](tiled-levels.md) for the level itself.
