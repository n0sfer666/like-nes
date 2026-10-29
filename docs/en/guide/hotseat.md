# Hotseat

English · [Русский](../../ru/guide/hotseat.md)

Two players at one machine, one on the keyboard or a pad, the other joining whenever they pick up a
second pad, leaving in the middle of a level and coming back. The engine splits this in two: the
**lobby** watches the devices and decides who is at the controls, outside the simulation; the
**seats** are part of the world and turn "player 2 is here" into a body on the street, the same on
every machine that replays the inputs. This page covers both and how Neon Rumble brings the second
player into a level that is already running.

The headers are `lobby.hpp` and `seat_state.hpp` in the namespace `framework::input`, `seats.hpp` in
`framework::core`, `seat_hash.hpp` in `framework::physics` and `seat_step.hpp` in `framework::brawl`.
They are installed with the SDK (see [The SDK and your own game](sdk-and-your-game.md)).

## The lobby

`Lobby(map, seats)` owns no devices; it reads the `DeviceState` the input engine already gathered
and changes the player assignments of an `ActionMap`. Each player has its own layout in the map;
`set_layout` refuses a key that another player already uses and reports it in `SharedInput`.

- `join(player, assign, shared)` seats a player on a known device, for example player 1 on the
  keyboard at start;
- `update(device)` runs once per tick, after `InputEngine::drain` and before `resolve`: a button of
  a free pad seats it on the lowest free seat, a key of player N's layout seats player N;
- `leave(player)` frees the seat;
- `state(player)`, `present(player)`, `lost(player)` and `paused()` say where each seat is.

A seat is `Free`, `Joining`, `Present`, `Lost` or `Resuming` (`SeatState`). `Joining` and `Resuming`
last one tick, so the game can greet the player once. When a pad is unplugged its seat turns `Lost`,
the lobby is `paused()` and the seat is **not** freed: the game stops and asks to reconnect the pad,
and a button on any free pad brings the player back. A player who wants out leaves with a button;
the lobby never drops a seat on its own.

Neon Rumble seats player 1 on the keyboard when it opens and reads both players every tick. A player
that is not present gets an empty command, and the leave action frees the seat:

<!-- snippet: games/neon-rumble/src/rumble_hotseat.cpp#hotseat-tick -->
```cpp
bool Hotseat::tick(uint32_t t, Players& out) {
    engine_.drain();
    lobby_.update(engine_.device());
    for (uint32_t p = 0; p < out.size(); ++p) {
        const int player = static_cast<int>(p);
        const input::InputFrame& f = engine_.resolve(t, player);
        out[p] = lobby_.present(player) ? command_of(engine_.buffer(player)) : PlayerCommand{};
        if (lobby_.present(player) && f.action_pressed(LEAVE)) {
            lobby_.leave(player);
            out[p] = PlayerCommand{};
        }
    }
    report(t);
    return !lobby_.paused();
}
```
<!-- /snippet -->

| action | player 1 | player 2 | pad, either player |
|---|---|---|---|
| move | W A S D | arrows | left stick |
| jump | Space, K | numpad 0 | A |
| punch, cross, kick | J, U, L | numpad 1, 4, 2 | X, Y, B |
| grab | H | numpad 5 | RB |
| block | I | numpad 3 | LB |
| dodge | O | numpad 6 | right stick press |
| leave | Backspace | numpad minus | Back |

Player 2 leaves with numpad minus, not Backspace: two players at one keyboard would share the key,
and the map would refuse the second layout.

Dodge has a second way in that needs no dodge button: hold block and flick the move axis left or
right, from rest to three quarters of its travel within 3 ticks. `dodge_flick` reads it off the last
frames of the input buffer, so a recording replays it tick for tick; the roll goes where the fighter
faces, whichever way the flick went, and a slow tilt keeps the block. A keyboard axis jumps to full
in one tick, so block plus a move key is a dodge too.

The game prints every change of a seat, for example `neon-rumble: seat tick 412: P2 joining`, then
`P2 present`; a lost pad prints `P2 lost the pad, paused`.

## Seats in the world

`core::Seats<N>` holds `N` seats, each a flag `present` and the `EntId` of its body; the brawl takes
`brawl::Seats`, which is `core::Seats<SEATS>` with **2** seats. It is part of the snapshot and of the
world hash (`physics::seats_hash`), so a rollback and a replay put the second player back exactly
where the live run had them.

`step_seats(seats, pool, inputs, spawns)` is the first system of a tick. The `present` flag of each
`BrawlInput` is the only thing it reads: a seat whose input turned present spawns the body the game
put in `spawns` for it, a seat whose input went away despawns its body. A body that was removed
elsewhere — knocked out, taken by the game — is replaced by a new one in the same tick, as long as
the seat stays present. `seat_commands` then copies each seat's input into the command of its body
and leaves every other body with an empty command; which attack a press means is the game's choice
(see [Bodies and hits](bodies-and-hits.md#the-tick-of-a-fight)).

<!-- snippet: games/neon-rumble/src/rumble_brawl.cpp#seat-step -->
```cpp
std::array<br::BrawlInput, br::SEATS> inputs{};
for (uint32_t p = 0; p < br::SEATS; ++p) inputs[p] = players[p].input;
br::step_seats(seats, pool, inputs, entry_points(*this));
remember(*this, 0);
std::array<br::Command, br::POOL_CAPACITY> commands{};
br::seat_commands(seats, pool, inputs, commands);
```
<!-- /snippet -->

## Joining a running level

The `spawns` body decides where the second player appears. On a street that the camera has already
left behind, the spawn point of the level is far out of view, so Neon Rumble moves it:

1. from the last checkpoint, keeping the distance between the spawn points of the two players;
2. then into the view of the camera, at least **32** pixels (`ENTRY_MARGIN`) inside its edge, and
   inside the band of the street the camera may still reach;
3. the new body is untouchable for **120** ticks (`ENTRY_IMMUNE`), two seconds.

<!-- snippet: games/neon-rumble/src/rumble_wave.cpp#wave-entry -->
```cpp
void Wave::entry(std::span<br::Body, br::SEATS> at, const br::DepthFloor& level_floor) const {
    const br::DepthFloor floor = platform.reach(level_floor, level, waves.cleared);
    const framework::scene::Checkpoint* cp = br::find_checkpoint(level, progress.checkpoint);
    const framework::Vec2 lead{at[PLAYER].pos.x, at[PLAYER].pos.z};
    for (br::Body& b : at) {
        b.immune = ENTRY_IMMUNE;
        if (cp == nullptr) continue;
        const fix32 x = cp->mark.at.x + (b.pos.x - lead.x);
        const fix32 z = framework::clamp_fix(cp->mark.at.y + (b.pos.z - lead.y), floor.band.z0, floor.band.z1);
        if (!on_floor(floor, x, z)) continue;
        b.pos.x = x;
        b.pos.z = z;
    }
    const fix32 half = framework::graphics::view_zone_half().x - fix32::from_int(ENTRY_MARGIN);
    const fix32 lo = framework::max_fix(camera.center.x - half, floor.band.x0);
    const fix32 hi = framework::min_fix(camera.center.x + half, floor.band.x1);
    for (br::Body& b : at) {
        const fix32 x = framework::clamp_fix(b.pos.x, lo, hi);
        if (on_floor(floor, x, b.pos.z)) b.pos.x = x;
    }
}
```
<!-- /snippet -->

When that point is inside a platform section and is already taken by a tile or by another body, the
engine puts the newcomer in the nearest free point instead: from the requested `x`, a body width at
a time, right first and then left, only inside the section. When no point is free, the body stays on
the street at the edge of the section, as a body walking in would (see [Camera and
sections](camera-and-sections.md#sections)).

The fight follows the number of players. With two the enemies get two attack tokens instead of one,
and a spawn point the level marks `players = 2` is used only while both are present; a change in the
number of players applies from the next wave, not the one already on the street.

`neon-rumble --wave --coop` plays the hotseat by itself: two bots walk `level1`, the second one
joins through the lobby like a player would, and inside the platform section it leaves and joins
again. It is the run the engine's checks use to judge that the second player appears in view.
