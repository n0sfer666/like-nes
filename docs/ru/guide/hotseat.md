<!-- en-sha256: 68b0f8015fddbe41bd38026bf2722e6183067107b8bf35d9672fc06311adeb77 -->

# Хотсит

[English](../../en/guide/hotseat.md) · Русский

Двое за одной машиной: один на клавиатуре или паде, второй подключается, когда возьмёт второй пад,
выходит посреди уровня и возвращается. Движок делит это надвое: **лобби** следит за устройствами и
решает, кто за управлением, вне симуляции; **места** — часть мира и превращают «второй игрок здесь»
в тело на улице, одинаково на каждой машине, которая проигрывает этот ввод. Эта страница описывает и
то и другое и то, как Neon Rumble вводит второго игрока в уже идущий уровень.

Заголовки — `lobby.hpp` и `seat_state.hpp` в пространстве имён `framework::input`, `seats.hpp` в
`framework::core`, `seat_hash.hpp` в `framework::physics` и `seat_step.hpp` в `framework::brawl`. Они
ставятся вместе с SDK (см. [SDK и своя игра](sdk-and-your-game.md)).

## Лобби

`Lobby(map, seats)` не владеет устройствами: он читает `DeviceState`, который уже собрал движок
ввода, и меняет назначения игроков в `ActionMap`. У каждого игрока в карте своя раскладка;
`set_layout` отказывает в клавише, которую уже занял другой игрок, и сообщает о ней в `SharedInput`.

- `join(player, assign, shared)` сажает игрока на известное устройство, например игрока 1 на
  клавиатуру при старте;
- `update(device)` зовётся раз в тик, после `InputEngine::drain` и до `resolve`: кнопка свободного
  пада сажает его на младшее свободное место, клавиша раскладки игрока N сажает игрока N;
- `leave(player)` освобождает место;
- `state(player)`, `present(player)`, `lost(player)` и `paused()` говорят, где каждое место.

Место бывает `Free`, `Joining`, `Present`, `Lost` или `Resuming` (`SeatState`). `Joining` и
`Resuming` длятся один тик, чтобы игра поприветствовала игрока один раз. Когда пад отключают, его
место становится `Lost`, лобби `paused()`, и место **не** освобождается: игра останавливается и
просит переподключить пад, а кнопка любого свободного пада возвращает игрока. Кто хочет выйти,
выходит кнопкой; сам лобби места не снимает.

Neon Rumble сажает игрока 1 на клавиатуру при открытии и читает обоих игроков каждый тик.
Отсутствующий игрок получает пустую команду, а действие выхода освобождает место:

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

| действие | игрок 1 | игрок 2 | пад, любой игрок |
|---|---|---|---|
| движение | W A S D | стрелки | левый стик |
| прыжок | Space, K | нампад 0 | A |
| джеб, кросс, пинок | J, U, L | нампад 1, 4, 2 | X, Y, B |
| захват | H | нампад 5 | RB |
| блок | I | нампад 3 | LB |
| уклон | O | нампад 6 | нажатие правого стика |
| выход | Backspace | минус нампада | Back |

Игрок 2 выходит минусом нампада, а не Backspace: двое за одной клавиатурой делили бы клавишу, и
карта отказала бы во второй раскладке.

У уклона есть второй вход без своей кнопки: удержать блок и рвануть ось движения влево или вправо —
из покоя до трёх четвертей хода за 3 тика. `framework::input::flick` читает это по последним кадрам буфера ввода,
поэтому запись воспроизводит уклон тик в тик; перекат идёт туда, куда смотрит боец, в какую бы
сторону ни был рывок, а медленный наклон оставляет блок. Ось клавиатуры встаёт в полную за один
тик, так что блок и клавиша движения — тоже уклон.

Игра печатает каждую смену места, например `neon-rumble: seat tick 412: P2 joining`, затем `P2
present`; отключённый пад печатает `P2 lost the pad, paused`.

## Места в мире

`core::Seats<N>` держит `N` мест, каждое — флаг `present` и `EntId` его тела; драка берёт
`brawl::Seats`, то есть `core::Seats<SEATS>` с **2** местами. Это часть снапшота и хеша мира
(`physics::seats_hash`), так что откат и реплей возвращают второго игрока ровно туда, где он был в
живом прогоне.

`step_seats(seats, pool, inputs, spawns)` — первая система тика. Читает она только флаг `present`
каждого `BrawlInput`: место, чей ввод стал присутствующим, порождает тело, которое игра положила для
него в `spawns`, место, чей ввод ушёл, убирает своё тело. Тело, снятое в другом месте — нокаутом,
игрой, — заменяется новым в том же тике, пока место присутствует. Затем `seat_commands` копирует
ввод каждого места в команду его тела и оставляет всем остальным телам пустую команду; какую атаку
значит нажатие, выбирает игра (см. [Тело и удары](bodies-and-hits.md#тик-драки)).

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

## Вход в идущий уровень

Где появится второй игрок, решает тело в `spawns`. На улице, которую камера уже оставила позади,
точка появления уровня далеко за кадром, поэтому Neon Rumble её сдвигает:

1. от последнего чекпоинта, сохраняя расстояние между точками появления двух игроков;
2. затем в вид камеры, не ближе **32** пикселей (`ENTRY_MARGIN`) к его краю, и внутрь полосы улицы,
   до которой камера ещё может дойти;
3. новое тело неуязвимо **120** тиков (`ENTRY_IMMUNE`), две секунды.

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

Когда эта точка внутри platform-секции и уже занята тайлом или другим телом, движок ставит новичка в
ближайшую свободную точку: от запрошенной `x` шагом в ширину тела, сначала вправо, затем влево,
только внутри секции. Если свободной точки нет, тело остаётся на улице у края секции, как осталось
бы тело, вошедшее пешком (см. [Камера и секции](camera-and-sections.md#секции)).

Драка следует числу игроков. При двоих врагам достаётся два жетона атаки вместо одного, а точка
появления, которую уровень пометил `players = 2`, работает только пока присутствуют оба; смена числа
игроков действует со следующей волны, а не с той, что уже на улице.

`neon-rumble --wave --coop` играет хотсит сам: два бота идут по `level1`, второй входит через лобби,
как вошёл бы игрок, а внутри platform-секции выходит и входит снова. Это прогон, которым проверки
движка судят, что второй игрок появляется в виде.
