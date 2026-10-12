<!-- en-sha256: e9b297eaa4589d8a0c29d5d69fbb07e749a856e742c1da3447109b293b5a7413 -->

# Сцены и уровни

[English](../../en/guide/scenes-and-levels.md) · Русский

Игра — это не один уровень: у неё есть меню, уровень, может быть экран паузы поверх уровня. Движок
держит их на **стеке сцен**, где у каждой сцены своё [расписание](tick-and-schedule.md), и грузит
уровень из бандла по имени одним вызовом, `load_level`, который отбивает битый уровень текстом, а не
крашем. Эта страница описывает и то и другое, время жизни загруженного уровня и меню Neon Rumble,
построенное на них.

Заголовки — `scene_stack.hpp`, `level_load.hpp`, `level_error.hpp` и `scene_level.hpp`, пространство
имён `framework::scene`. Они ставятся вместе с SDK
(см. [SDK и своя игра](sdk-and-your-game.md)).

## Стек сцен

`Scene` — три поля: указатель на собранный `Schedule`, `void* user`, который никуда не передаётся и
нужен игре, чтобы найти свой объект, и флаг `rollback` — идёт ли сцена в сессии отката. Меню не идёт,
драка идёт.

`SceneStack` держит до **4** сцен. `push`, `pop` и `replace` в момент вызова стек не меняют. Они
встают в очередь, до **8** за тик, и `apply()` выполняет их в порядке подачи. `run(tick)` зовёт
`apply()` и затем исполняет расписание только **верхней** сцены. Сцены под ней стоят, поэтому
сцена паузы, положенная поверх уровня, замораживает уровень, а `pop` продолжает его ровно с того
места, где он остановился. Посреди шага сцена не меняется никогда.

Команда проверяется в момент подачи — по глубине, которая будет у стека после разбора очереди, —
поэтому вызывающий узнаёт результат сразу. Отбитая команда ничего не меняет:

| `SceneResult` | `scene_reason()` | Когда |
|---|---|---|
| `BadScene` | `scene without a built schedule` | `push` или `replace` сцены с пустым или несобранным расписанием |
| `Overflow` | `scene stack is full` | `push` поверх 4 сцен, с учётом команд в очереди |
| `Empty` | `scene stack is empty` | `pop` или `replace` без сцены, с учётом команд в очереди |
| `QueueFull` | `too many scene commands in one tick` | девятая допустимая команда до `apply()` |
| `Rollback` | `scene command from a rollback scene or a resim` | см. следующий раздел |

Стек и его очередь не входят ни в снапшот, ни в хеш мира. Уровень, на который указывает сцена,
входит.

## Команды из rollback-сцены

Сессия отката заново симулирует уже пройденные тики. Если бы система сцены драки звала `replace`,
когда уровень пройден, каждый ресим этого тика подавал бы команду заново, а команда из
предсказанного тика применилась бы и после коррекции, отменившей этот тик. Стек не может отличить
одно от другого, потому что он вне снапшота.

Поэтому стек такие команды отбивает. Пока `run` исполняет сцену с `rollback = true` и пока `resim`
исполняет любую сцену, `push`, `pop` и `replace` возвращают `Rollback` и оставляют стек как был.
`resim(tick)` — вход для сессии отката: он исполняет расписание верхней сцены без `apply()`, потому
что очередь подтверждённого тика посреди ресима применяться не должна. `apply`, `run` или `resim`,
вложенные в исполняемое расписание, ничего не делают.

Команды стека игра подаёт своим кодом, **после** `run`, по подтверждённому тику: «уровень пройден»
или «загрузка не удалась» читается из состояния, а не посылается из системы. Neon Rumble подаёт
их после шага:

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

## Загрузка уровня

`load_level(bundle, name, out, error)` находит в бандле уровень `name` и заполняет `out` всем, что
нужно драке:

- таблицу `visual` и её карту `name`, сетку коллизий из таблицы `tilemap`;
- таблицу `objects`, прочитанную в `LevelObjects` по словарю классов ниже, и границы камеры из
  объекта `bounds`, а без него — по всей карте;
- текстуры, которые используют визуальные слои;
- таблицу `clips` и по одной таблице бойца на каждый различный `kind` спавнов волн, по имени
  `<kind>_fighter`. И то и другое открывается, только если у уровня есть волны.

Он возвращает `true` либо `false` с заполненными `error.fault` и `error.text`. Текст лежит в
фиксированном буфере на 192 байта, и движок его не печатает: показывать его — решение игры. После
отказа `out` снова пустой `Level`, а не уровень, загруженный наполовину.

| `LoadFault` | Пример `error.text` |
|---|---|
| `NoTable` | `objects table: missing` |
| `BadTable` | `visual table does not open` |
| `NoLevel` | `no level nope in the visual table` |
| `BadObjects` | `level lvl: checkpoint 'cp9' (id 40) repeats checkpoint order 1 of object id 14` |
| `Texture` | `level lvl: texture <guid в hex>: wrong texture format` |
| `UnknownKind` | `level lvl: spawn a1 w0 kind boss: no table boss_fighter` |
| `BadFighter` | `adler_fighter: no clip adler/jab in the clips table` |
| `Capacity` | `level lvl needs more than 8 fighter kinds` (или 8 textures) |

### Классы объектов

Как объекты попадают в бандл, описывает страница [Уровни в Tiled](tiled-levels.md).
`load_level` придаёт смысл этим классам и пропускает все остальные — их толкует игра:

| Класс | Форма | Свойства |
|---|---|---|
| `spawn` | точка | без `wave` — спавн игрока. С `wave` (не меньше 0): `arena` — id арены уровня; `kind` — непустой; необязательное `players` (не меньше 1) |
| `checkpoint` | точка | `order`, от 0 до 32767, уникальный в уровне |
| `arena` | прямоугольник | `id`, от 0 до 32767, уникальный в уровне |
| `section` | прямоугольник | `mode`, `belt` или `platform` |
| `autoscroll` | прямоугольник | `axis`, `x` или `y`, и `speed` |
| `depth_band`, `wall`, `kill` | прямоугольник | нет |
| `conveyor` | прямоугольник | `speed` |
| `path` | многоугольник | `speed`, `loop`; не больше 16 вершин |
| `rope` | точка | `length` |
| `bounds` | прямоугольник | нет; не больше одного на уровень |

Нет свойства, свойство не того типа или вне своего диапазона, объект не той формы, ссылка на
несуществующую арену — отказ `BadObjects`, который называет объект и его id. Диапазон `checkpoint.order` не произволен: прогресс уровня хранит взятый `order` в 16 битах, где
−1 значит «ни одного не взято», поэтому `order` больше 32767 не взялся бы никогда. Два чекпоинта с
одним `order` отбиваются, потому что прогресс хранит только `order`, а респаун ищет первую точку с
ним: игрок, взявший вторую, вернулся бы у первой.

## Время жизни Level

`Level` ничего не копирует из бандла. Его имя, `kind` каждого бойца, пиксели текстур и строки всех
таблиц — виды в отображённый бандл, поэтому `Level` не должен пережить отображение, из которого
загружен. Сам бандл хранится внутри как `level.bundle`, и `load_level` принимает его **по значению**:
`load_level(level.bundle, "level2", level, error)` безопасно перезагружает тот же `Level`, хотя
первым делом `load_level` очищает `out`.

`Level` не перемещается, когда на нём открыта драка: драка держит указатели на него, а расписание
сцены — указатели на данные своих систем. Поэтому Neon Rumble держит уровень, бойцов, драку и сцену
вместе в одном объекте в куче, `Play`, и грузит его только при входе:

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

`Schedule` и `user` сцены должны жить, пока сцена в стеке **или в очереди**: до `apply()` поданному
`replace` нужны обе: старую сцену ещё держит стек, а новую уже держит очередь. Neon Rumble отпускает свой уровень в `apply()`, когда на
вершине снова меню:

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

## Меню Neon Rumble

Окно всегда открывается на меню. В нём три пункта, нарисованных шрифтом титров: `level level1`,
`wave level1` и `credits`. **↑** и **↓** выбирают, **Enter** входит. Вход в уровень зовёт
`enter_level` выше и затем `replace` сценой уровня, так что следующий кадр начинается в уровне.
**Esc** в уровне заменяет его меню, и следующий вход грузит уровень из бандла заново.

Если загрузка отбита, игра остаётся в меню, показывает текст отказа над пунктами и печатает его в
stdout строкой `menu:`:

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

`--level <имя>` задаёт уровень пунктов меню, а `--wave` выделяет пункт `wave`. У прогона headless
меню нет: он сразу входит в выбранный пункт. Отбитая загрузка печатает ту же строку среди прочих и
завершает прогон кодом 0 — так CI проверяет путь обратно в меню на трёх ОС:

```sh
neon_rumble --headless --level nope --frames 1
```

```text
neon-rumble: menu: no level nope in the visual table
```
