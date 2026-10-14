<!-- en-sha256: 0ef3623a635524c7f008e49414d872a103533ca8d4099e18e4865119d3204856 -->

# Камера и секции

[English](../../en/guide/camera-and-sections.md) · Русский

В битэмапе камера — правило игры, а не украшение: она запирает игроков в арене, пока волна не
побита, держит их в виде и толкает вперёд в автоскролле. Поэтому камера живёт в симуляции, в
снапшоте и хеше мира, и уровень вместе с ней решает, где начинается драка. Уровень может ещё и
переключаться с улицы с глубиной на платформер и обратно. Эта страница описывает камеру, секции,
арены с их волнами и чекпоинты.

Объекты, которые читают эти правила, — `arena`, `section`, `autoscroll`, `checkpoint`, `bounds` и
`spawn` — размечаются в Tiled и описаны в [Сценах и уровнях](scenes-and-levels.md#классы-объектов);
конвейеры, лифты, тросы и снаряды описаны там же. Заголовки — `stage_camera.hpp`, `stage_step.hpp`,
`stage_sections.hpp`, `platform_world.hpp`, `stage_waves.hpp`, `wave_step.hpp`, `wave_facts.hpp`,
`next_arena.hpp`, `level_progress.hpp` и `checkpoint_step.hpp`, пространство имён
`framework::brawl`. Они ставятся вместе с SDK (см. [SDK и своя игра](sdk-and-your-game.md)).

## Шаг уровня

`step_stage` — это [`step_brawl`](bodies-and-hits.md#тик-драки) для уровня. За один вызов он двигает
лифты и конвейеры, тросы, вводит в их секцию тела, появившиеся в этом тике, двигает каждое тело,
меняет секции, шагает камеру и разрешает удары. Волны, чекпоинты и смерти идут после него,
отдельными вызовами игры. Neon Rumble гоняет уровень так; тела новой волны порождает игра по фактам,
которые вернул `step_waves`:

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

## Камера

`StageCamera` держит `center` вида, запертую `arena` (`NO_ARENA`, -1, когда её нет) и состояние
автоскролла. Тряски здесь нет: это украшение `graphics::Camera` у отрисовки, и симуляцию она не
трогает. `step_camera` работает внутри `step_stage`:

1. живых игроков нет — камера стоит;
2. `center.x` — середина охвата живых игроков по `x`;
3. в `belt`-секции `center.y` не следует; в `platform`-секции он следует за серединой игроков на
   экране с мёртвой зоной `PlatformWorld::dead_y` пикселей (24 в Neon Rumble);
4. автоскролл двигает центр (см. ниже);
5. центр зажимается в запертую арену, затем в `bounds` уровня;
6. **поводок** возвращает каждого живого игрока в зону вида (**384 × 216**): его скорость по зажатой
   оси становится 0.

Сама игра ставит центр только при открытии уровня: Neon Rumble ставит его в игрока 1, зажатого, и
пишет тот же `x` в `StageWaves::seen`.

**Автоскролл.** Пока центр внутри прямоугольника `autoscroll`, камера проезжает его один раз со
скоростью `speed` по оси `axis`; когда игроки возвращаются, второго проезда нет. Когда проезд
кончается, камера возвращается к игрокам по `max(|speed|, разрыв / 8)` за тик, так что не прыгает и
не тянется. Игрок, которого поводок автоскролла по `x` вжал в стену, получает `Body::crushed`; что
это значит — урон, респаун, — решает игра.

## Секции

Прямоугольник `section` задаёт режим уровня по `x`: выигрывает первая секция с `x0 ≤ x < x1`, а вне
всех секций уровень — `belt`. В `platform`-секции тело ведёт контроллер персонажа движка, тот же,
что у платформера; `PlatformWorld` держит его сцену коллизий, корпус, профиль движения и длину тика.
Neon Rumble берёт корпус 8 × 32 половинного размера, 60 тиков в секунду и профиль по умолчанию.
Состояние каждого тела в platform-секции — его `EntId` и контроллер — лежит в `StageSections`, у
которого свой хеш.

- Тело меняет секцию только на опоре; в воздухе смена ждёт приземления.
- Когда точка, на которую оно встало бы в новой секции, занята тайлом или другим телом, оно не
  входит: его `x` держится на границе.
- Тело, появившееся прямо в platform-секции на занятой точке, сдвигается в ближайшую свободную точку
  внутри секции, шагом в ширину тела, сначала вправо, затем влево (см.
  [Хотсит](hotseat.md#вход-в-идущий-уровень)).
- Удары попадают только между телами одного режима.

## Арены и волны

`StageWaves` держит последнюю зачищенную арену, текущую волну, паузу и позицию камеры `seen`.
`step_waves(waves, camera, pool, kinds, seats, level, rules, facts)` идёт после шага уровня:

- Когда арена не заперта, берётся **следующая арена** — с наименьшим `id` больше последней
  зачищенной. Она запирается, когда центр дошёл до точки, в которую его там зажмёт, или пересёк её —
  считая от `seen` до нового центра, так что быстрая камера её не проскочит. Ставится
  `facts.locked`, и стартует первая волна.
- Волна стартует со всеми `spawn` этой арены и волны, у которых `players` не больше числа
  присутствующих мест. Их перечисляет `facts.starts`; тела порождает игра (до ёмкости пула,
  остальные — в `facts.dropped`). Число игроков читается при старте волны, так что вошедший или
  вышедший игрок меняет следующую волну, а не эту.
- Волна зачищена, когда не осталось бойцов команды, отличной от 0 (`facts.cleared`). Следующая волна
  идёт через `WaveRules::lull` тиков (120 в Neon Rumble); после последней арена отпускается
  (`facts.released`), камера свободна, и `cleared` двигается дальше.

`next_arena(level, cleared)` — то же правило в виде функции; она возвращает `nullptr`, когда все
арены зачищены. Игра, которой нужно знать, впереди ли ещё драка, зовёт её, а не держит свою копию.
Neon Rumble расширяет улицу до конца platform-секции, только когда арен не осталось:

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

## Чекпоинты

`LevelProgress` держит `order` последнего взятого чекпоинта. `step_checkpoints` берёт чекпоинт,
когда живой игрок стоит на опоре на его `x` или дальше, и только с `order` больше текущего.
`find_checkpoint(level, order)` возвращает его — для респауна игры и для игрока, вошедшего позже.

В `level1` Neon Rumble улица идёт от 104 до 1560, арена 0 — 520..968, арена 1 — 1176..1560;
platform-секция с конвейером — от 1568 до 2200. Сам уровень — в [Уровнях в Tiled](tiled-levels.md).
