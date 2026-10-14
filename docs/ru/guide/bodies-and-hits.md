<!-- en-sha256: 171b3b5d390b9b70a634376ca3e1c2409275fa999153b0ec952cbc08b0bdbf56 -->

# Тело и удары

[English](../../en/guide/bodies-and-hits.md) · Русский

Битэмап — это толпа тел на улице с глубиной: игроки, враги, брошенные ножи, ящики. Движок держит их
в одном пуле, двигает по одним правилам каждый тик и решает, какой удар попал, обычной геометрией с
фиксированной точкой, так что две машины, получившие одинаковый ввод, держат один и тот же мир бит в
бит (см. [Детерминизм и арифметика с фиксированной точкой](determinism.md)). Эта страница описывает
тело, пул, тик драки, правила попадания и файл `.fighter`, которым описывается боец.

Заголовки — `depth_body.hpp`, `brawl_body.hpp`, `body_pool.hpp`, `brawl_input.hpp`,
`brawl_step.hpp`, `hit_events.hpp`, `reaction.hpp`, `fighter_read.hpp` и `archetype.hpp`,
пространство имён `framework::brawl`. Они ставятся вместе с SDK (см. [SDK и своя
игра](sdk-and-your-game.md)).

## Тело на улице с глубиной

`DepthBody` — положение и скорость по трём осям, все `fix32` в пикселях: `x` идёт вдоль уровня, `z`
— глубина улицы, `y` — высота над полом, вверх положительна. Экран видит `(x, z - y)`, и считает это
только отрисовка: прыжок меняет `y`, шаг вглубь улицы меняет `z`, и симуляция их никогда не
смешивает.

`Body` добавляет остальное: `EntId`, `facing`, `team` и `owner`, `hp`, архетип `kind`, клип и ход с
их тиками `elapsed`, `hitstop`, `immune`, реакцию с её тиками, список тел, которые задел текущий
замах, шаг цепочки, отложенную атаку, бег и захват. Команда `0` — игроки.

`BodyPool` держит до **26** тел (`POOL_CAPACITY`: 2 игрока, 8 врагов, 16 снарядов и пропсов).
`spawn` выдаёт `EntId`, чей `seq` только растёт, `despawn` убирает тело, `find` ищет его.
Освобождённый `seq` не переиспользуется, так что старый id не укажет на новое тело; счётчик
`next_seq` входит в снапшот, а пул обходится и хешируется в порядке `seq`.

## Тик драки

Тело ведёт `Command`: `BrawlInput` и атака, которую для него выбрала игра. `BrawlInput` — это
`MoveInput` (стик по двум осям), биты `button::ATTACK`, `JUMP`, `GRAB`, `BLOCK`, `DODGE` и `RUN` (с
0 по 5) и флаг `present`; пустой `BrawlInput{}` значит, что за управлением никого нет. `BrawlWorld`
— то, что читает каждое тело и не пишет никто: архетипы, пол, `HitRules`, а для уровня — объекты
уровня и платформенный мир (см. [Камера и секции](camera-and-sections.md)).

`step_brawl(pool, commands, world, events)` — это `step_bodies`, затем `resolve_hits`: сначала
двигаются все тела, потом удары собираются против сдвинутых тел и применяются разом, так что порядок
пула никогда не решает, кто ударил первым. Полный порядок систем тика, которому следует шаг уровня
битэмапа:

1. ввод и флаг `present` мест ([Хотсит](hotseat.md));
2. ИИ заполняет команды врагов;
3. реакции и состояния;
4. движение;
5. коллизия и переходы секций;
6. камера;
7. удары: сбор, затем применение;
8. клипы: `elapsed + 1`, `hitstop - 1`;
9. волны, чекпоинты и смерти.

Neon Rumble собирает команды двух игроков в игре и отдаёт шагу весь пул. Удар игрока выбирается по
его кнопкам; у врагов он уже есть от ИИ:

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

## Когда удар попадает

Каждый кадр клипа несёт до четырёх боксов удара и боксов попадания (см. [Анимации в
Aseprite](aseprite-animations.md#боксы-удара-боксы-попадания-и-пивот)). Бокс смотрит туда же, куда
тело: его `x` — `x + facing * rx`, по высоте он идёт от `y - ry - h` до `y - ry`. Бокс удара
задевает бокс попадания, когда они пересекаются по `x` и по высоте, оба как полуоткрытые отрезки,
**и** глубины близки: `|z_атакующего - z_цели| < depth_атакующего + depth_цели`, строго. Боксы,
которые лишь касаются краем, не попадают.

Удар отбивается до геометрии, когда цель — сам атакующий, тело, которым атакующий владеет, или тело
той же команды при `HitRules::friendly_fire == false`. Брошенное тело и снаряд берут команду и
владельца того, кто их бросил. Один замах бьёт тело один раз: id задетых хранятся на время хода (до
**16**). Тело с `immune > 0` не бьётся (его ставит игра, например при респауне; `immune = N` держит
тело закрытым следующие `N - 1` сборов). Бить можно только тело в реакции `None` или `Hurt`; `Fall`,
`Down`, `Getup`, `Dodge` и `Thrown` — нельзя, про `Block` ниже.

Все удары тика применяются вместе. Урон складывается, `hp` останавливается на 0. `hitstop` —
наибольший из событий, а не сумма. Отброс и hitstun берутся от события с наибольшим hitstun; при
равенстве выигрывает атакующий с меньшим `seq`, затем первый бокс. Цель получает `vx = facing *
knock_x`, `vz = 0` и `vy = knock_y`, когда он больше 0 или цель в воздухе. За тик хранится до **64**
событий (`MAX_HIT_EVENTS`); остальные считает `HitEvents::dropped`.

`launch` отправляет цель в `Fall` до приземления, затем в `Down` на `down` тиков, затем в `Getup` на
`getup` тиков. **Блок** (`BLOCK` удержан, на земле) гасит `light` или `heavy` спереди: ни урона, ни
реакции, хитстоп обоим, а цель отъезжает назад на половине отброса на время hitstun. Удар сзади и
`launch` проходят. **Уклон** делает тело неуязвимым на `dodge` тиков и двигает его со скоростью
`facing * run_x`. **Бег** включается двойным нажатием в пределах `run_tap` тиков и идёт со скоростью
`run_x`; ход с `slide` катится дальше на `facing * run_x`.

**Цепочка** — список ходов, по которым проходит кнопка атаки. Нажатие держится в буфере `buffer`
тиков; следующий шаг стартует, когда в буфере есть нажатие, текущий клип дошёл до события `cancel` и
текущий ход попал. Каждому шагу, кроме последнего, нужно событие `cancel` в клипе. **Захват**
проверяет свой первый бокс удара против бокса `push` цели (нижняя половина её первого бокса
попадания на кадре 0), проходит сквозь блок и переводит цель в `Thrown` у якоря бросающего;
брошенное тело летит со скоростью `knock_x`/`knock_y` захвата и бьёт других ходом `throw`. Два
бойца, схватившие друг друга в одном тике, оба отпускают. Ход с `hits_down yes` бьёт только тело в
`Down`.

## Файл .fighter

Боец — текстовый файл той же `|`-грамматики, что и манифест ассетов (см. [Уровни в
Tiled](tiled-levels.md)). Манифест кладёт его в бандл таблицей с именем `<name>_fighter`, например
`fighter | adler_fighter | fighters/adler.fighter`. Клипы ищутся по имени `<sheet>/<tag>` в листе
`clips` уровня.

Шапка файла:

| ключ | диапазон | смысл |
|---|---|---|
| `sheet` | имя | лист спрайтов, равен имени бойца |
| `speed_x`, `speed_z`, `run_x`, `jump_vy` | 0..16 | шаг вдоль и поперёк улицы, бег, прыжок, px за тик |
| `gravity` | >0..16 | px за тик за тик |
| `depth` | >0..64 | половина глубины тела для ударов |
| `hp` | 1..9999 | здоровье при появлении |
| `down`, `getup`, `buffer`, `run_tap`, `dodge` | 1..120 | тики |
| `chain` | 1..8 ходов | цепочка атаки; последний ход — финишер |

Затем по строке `move | <name> | hit0` … `hit3` на каждый бокс удара хода:

| ключ | диапазон | смысл |
|---|---|---|
| `clip` | имя | клип хода, по умолчанию имя хода |
| `type` | `light`, `heavy`, `launch`, `grab`, `throw` | как реагирует цель |
| `damage` | 0..999 | |
| `depth` | >0..64 | дальность этого бокса по глубине |
| `hitstop`, `hitstun` | 0..120 | тики |
| `knock_x`, `knock_y` | -16..16 | `knock_x > 0` толкает прочь по взгляду атакующего, `knock_y > 0` — вверх |
| `hits_down`, `slide` | `yes`, `no` | |

`adler` из Neon Rumble ходит на 2 и 1, бегает на 3, прыгает на 6 при гравитации 0.5, глубина 6, 100
hp и цепочка `jab jab kick`; его `jab` — `light`, урон 6, глубина 8, hitstop 4, hitstun 10, отброс 1
и 0 — см.
[`games/neon-rumble/fighters/adler.fighter`](../../../games/neon-rumble/fighters/adler.fighter):

```text
move | jab | hit0
type      | light
damage    | 6
depth     | 8
```

`FighterTable::open` читает забейканную таблицу, `make_archetype(table, clips, out, error)`
превращает её в `Archetype` и проверяет, что в листе есть клипы, нужные каждому бойцу: `idle`,
`walk`, `jump`, `hurt`, `fall`, `down`, `getup`, `block`, `dodge`, `throw` и `thrown`. Битый файл
отбивается строкой текста, например `unknown key 'speeed_x'`, `… is outside the range the engine
accepts`, `type must be light, heavy, launch, grab or throw`, `chain must name 1 to 8 moves` или
`move 'jab' plays two clips`. Neon Rumble открывает каждого бойца так:

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
