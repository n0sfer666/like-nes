<!-- en-sha256: dd53d15abf2e9311629bf4c045680ccd82ff5ab66c5d7cc666443e2438f83502 -->

# Тик и расписание

[English](../../en/guide/tick-and-schedule.md) · Русский

Кадр симуляции — это **тик**: фиксированный шаг времени, за который каждая зарегистрированная
система выполняется ровно один раз, в порядке, который движок ВЫВОДИТ, а не запоминает. Эта
страница про этот порядок: чем он задан и почему он не совпадает с порядком регистрации.

Всё показанное ниже взято из [`docs/examples/schedule_order.cpp`](../../examples/schedule_order.cpp),
который CI собирает и запускает на трёх операционных системах. Вывод в конце страницы — то, что эта
программа печатает на самом деле.

## Стадии — крупная сетка

Стадий пять, и игры своих не добавляют:

| Стадия | Что там живёт |
|---|---|
| `Input` | сырое состояние устройств, превращённое в действия игрока |
| `PreSim` | намерения из действий: желаемая скорость, запросы прыжка и выстрела |
| `Sim` | физика и правила мира — то, что попадает в хеш симуляции |
| `PostSim` | производное от нового состояния: камера, анимация, звуковые события |
| `Present` | подготовка к отрисовке; в хеш симуляции не входит |

Система из более ранней стадии всегда идёт раньше системы из более поздней. Это всё, что дают
стадии; внутри стадии порядок задают зависимости.

## Система — это функция и её контекст

Системы не видят друг друга. Каждая получает тот указатель, с которым была зарегистрирована:

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

## Порядок регистрации — не порядок исполнения

`after` называет системы, после которых обязана идти эта. Ничьи — системы одной стадии, между
которыми нет ни одного ребра, — разрываются по имени, так что у набора систем ровно один
допустимый порядок:

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

Это важнее, чем кажется. Модули регистрируются из разных единиц трансляции, а порядок статической
инициализации между единицами стандарт не определяет — он зависит от порядка объектов на строке
линкера. Расписание, читающее порядок регистрации, давало бы другой хеш симуляции от перестановки
двух библиотек в CMake, и найти это можно было бы только сравнением двух сборок.

`build()` называет причину отказа: неизвестная зависимость, зависимость из более поздней стадии
(неудовлетворима по построению), дубль имени, цикл. `error_system()` называет систему, на которой
разбор остановился.

## Как это запускается

<!-- snippet: docs/examples/schedule_order.cpp#run -->
```cpp
// One call runs every stage in order. `run_stage` runs a single one -- that is how a fixed-step
// loop interleaves Sim with rendering.
framework::Tick tick;
tick.dt = fix32::from_float(1.0 / 60.0);
for (tick.index = 0; tick.index < 2; ++tick.index) forward_schedule.run(tick);
```
<!-- /snippet -->

`dt` — [число с фиксированной точкой](determinism.md), а не float, потому что попадает в хеш
симуляции.

Программа целиком печатает:

<!-- snippet: docs/examples/schedule_order.out -->
```text
order: read_pad aim resolve_hits apply_damage move_camera
reversed registration gives the same order: yes
trace after two ticks: iahdciahdc
```
<!-- /snippet -->

`apply_damage` лексикографически раньше `resolve_hits` и всё равно идёт после неё; переворот всех
вызовов `add()` не меняет ничего. Ровно ради этого свойства расписание и существует.
