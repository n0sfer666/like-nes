#include <cstdio>

#include "../tilemap/query.hpp"
#include "controller.hpp"
#include "platform_args.hpp"

// Гейт ОДНОСТОРОННЕЙ ПЛАТФОРМЫ (вертикаль 3, шаг B): снизу проходят, сверху стоят, по команде
// «вниз + прыжок» спускаются — и ни одно из трёх не должно случаться со сплошным полом.
//
// Своей целью и своей раскладкой по той же причине, что у гейтов склона и прощения: голден
// траектории идёт по сцене без единой платформы, то есть про них не утверждает ничего, а вопрос
// здесь мерится высотой ног и признаком опоры, а не хешем.
//
// Каждое утверждение ПАРНОЕ, и пара тут несущая: «прошёл насквозь» — правда и про персонажа,
// который никуда не долетел, а «спустился» — правда и про того, кому просто не выдали прыжок.
// Поэтому тот же прогон гоняется по раскладке, где платформа заменена СПЛОШНЫМ тайлом: расходятся
// они ровно одним битом карты.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::character;

fix32 tick_dt() { return fix32::from_int(1) / fix32::from_int(60); }
fix32 fx(int v) { return fix32::from_int(v); }

constexpr fix32 TILE = fix32::from_int(16);
constexpr fix32 HALF_W = fix32::from_int(6);
constexpr fix32 HALF_H = fix32::from_int(10);

// Платформа — ряд 3 (y от 48 до 64), пол — ряды 6 и 7 (y от 96). Просвет между ними 32 юнита при
// росте персонажа в 20: стоящий на полу не задевает платформу телом, и «прошёл снизу» значит именно
// проход, а не то, что он там и так помещался.
constexpr fix32 PLATFORM_TOP = fix32::from_int(48);
constexpr fix32 FLOOR_TOP = fix32::from_int(96);

CharacterHull make_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(HALF_W, HALF_H));
    return h;
}

constexpr tilemap::TileFlags ONEWAY = tilemap::TILE_SOLID | tilemap::TILE_ONEWAY;

// Платформа ПРЯМО НАД полом: её низ совпадает с верхом каменного ряда, и спустившийся долетает до
// пола за семь тиков — то есть внутри окна буфера (6 тиков по профилю). Раскладка заведена ради
// одного утверждения: спуск обязан ГАСИТЬ запомненное нажатие. На раскладке с высокой платформой
// оно не наблюдаемо вовсе — буфер успевает истечь в полёте, — и строка, его гасящая, выглядела бы
// мёртвой ровно до первой игры, где платформа висит низко.
tilemap::TileGrid make_stacked() {
    tilemap::TileGrid g({fix32{}, fix32{}}, TILE, 12, 8);
    g.fill(0, 6, 12, 8, tilemap::TILE_SOLID);
    g.fill(3, 5, 9, 6, ONEWAY);
    return g;
}

tilemap::TileGrid make_level(tilemap::TileFlags platform) {
    tilemap::TileGrid g({fix32{}, fix32{}}, TILE, 12, 8);
    g.fill(0, 6, 12, 8, tilemap::TILE_SOLID);
    g.fill(3, 3, 9, 4, platform);
    return g;
}

// Ноги персонажа — низ хитбокса плюс зазор, которым его отставляет скольжение. Мерить позицией
// центра значило бы записывать половину роста в каждое утверждение.
fix32 feet(const Character& c) { return c.position.y + HALF_H + SKIN; }

struct Input {
    bool jump;
    bool down;
};

struct Run {
    fix32 highest;      // САМЫЕ высокие ноги за прогон (наименьшая y)
    fix32 final_feet;
    bool ground_at_end;
    bool lost_ground;   // опора терялась хоть раз
};

// Прогон `ticks` тиков с вводом, который задаётся функцией от НОМЕРА тика: спуск и прыжок — это
// фронт, то есть событие одного тика, и передавать его флагом на весь прогон значило бы проверять
// зажатую кнопку вместо нажатия.
template <typename Fn>
Run run(const tilemap::TileGrid& g, fix32 start_feet, uint32_t ticks, Fn input,
        uint32_t buffer_ticks = default_profile().buffer_ticks) {
    MoveProfile raw = default_profile();
    raw.buffer_ticks = buffer_ticks;
    const MoveProfile p = sanitize(raw);
    const MoveDerived d = derive(p, tick_dt());
    const CollisionScene s{nullptr, &g};
    Character c;
    c.position = {fx(88), start_feet - HALF_H - SKIN};
    c.on_ground = probe_ground(s, make_hull(), c.position, p.max_slope);
    c.state = c.on_ground ? MoveState::Ground : MoveState::Air;
    Run r{feet(c), feet(c), c.on_ground, false};
    for (uint32_t t = 0; t < ticks; ++t) {
        const Input i = input(t);
        MoveInput in;
        in.move_x = fix32{};
        in.jump_held = i.jump;
        in.down_held = i.down;
        step(s, make_hull(), p, d, in, tick_dt(), c);
        r.highest = min_fix(r.highest, feet(c));
        r.lost_ground = r.lost_ground || !c.on_ground;
    }
    r.final_feet = feet(c);
    r.ground_at_end = c.on_ground;
    return r;
}

Input idle(uint32_t) { return {false, false}; }

// Падение сверху. Платформа обязана поймать — иначе всё остальное здесь про дырку в полу.
void test_falls_onto_it() {
    const Run r = run(make_level(ONEWAY), fx(16), 40, idle);
    std::printf("  fall:   feet=%.3f ground=%d\n", r.final_feet.to_double(),
                r.ground_at_end ? 1 : 0);
    check(r.ground_at_end, "falling onto a one-way platform lands on it");
    check(r.final_feet < PLATFORM_TOP + fx(1) && PLATFORM_TOP - fx(1) < r.final_feet,
          "and stops at its top, not somewhere inside the level");
}

// Проход снизу. Прыжок с пола (высота профиля 64 при просвете 48) обязан вынести ноги ВЫШЕ верха
// платформы, а на спуске платформа обязана поймать.
void test_jumps_through_from_below() {
    const auto press = [](uint32_t t) { return Input{t < 12, false}; };
    const Run one = run(make_level(ONEWAY), FLOOR_TOP, 90, press);
    const Run solid = run(make_level(tilemap::TILE_SOLID), FLOOR_TOP, 90, press);
    std::printf("  below:  oneway highest=%.3f final=%.3f | solid highest=%.3f final=%.3f\n",
                one.highest.to_double(), one.final_feet.to_double(), solid.highest.to_double(),
                solid.final_feet.to_double());
    check(one.highest < PLATFORM_TOP, "a jump from below carries the feet above the platform top");
    check(one.ground_at_end && one.final_feet < PLATFORM_TOP + fx(1) &&
              PLATFORM_TOP - fx(1) < one.final_feet,
          "and the way down lands on the platform");
    // Пара: ТОТ ЖЕ прыжок в ту же геометрию, но по сплошному тайлу, обязан упереться снизу и
    // вернуться на пол. Без неё «прошёл насквозь» доказывало бы лишь то, что прыжок высокий.
    check(!(solid.highest < PLATFORM_TOP),
          "pair: the same jump into a solid tile is stopped below");
    check(FLOOR_TOP - fx(1) < solid.final_feet, "pair: and it comes back down to the floor");
}

// Спуск по команде: «вниз» зажат, прыжок нажимается на тике 2 фронтом.
Input drop_at_two(uint32_t t) { return {t == 2, true}; }

void test_drops_through_on_command() {
    const Run one = run(make_level(ONEWAY), PLATFORM_TOP, 90, drop_at_two);
    std::printf("  drop:   feet=%.3f highest=%.3f ground=%d\n", one.final_feet.to_double(),
                one.highest.to_double(), one.ground_at_end ? 1 : 0);
    check(FLOOR_TOP - fx(1) < one.final_feet && one.final_feet < FLOOR_TOP + fx(1),
          "'down + jump' on a one-way platform drops through it and onto the floor below");
    // Запас в юнит, а не строгое сравнение с верхом: покой стоящего гуляет на один разряд Q16.16
    // вверх (замер — 47.999985 при верхе 48), а прыжок в этом профиле поднимает на 64 — различать
    // их юнитом можно, разрядом нельзя.
    check(!(one.highest < PLATFORM_TOP - fx(1)), "and the same press does not launch a jump");
    // Пара: ТОТ ЖЕ ввод на СПЛОШНОМ полу обязан дать обычный прыжок. Съеденный прыжок игрок читает
    // как пропавшее нажатие, а не как правило, и без этой пары спуск чинился бы отменой прыжка.
    // Стоит персонаж на КАМЕННОМ полу той же раскладки — платформа над ним остаётся односторонней,
    // и прыжок проходит её насквозь, то есть меряется он высотой, а не потолком.
    const Run on_solid = run(make_level(ONEWAY), FLOOR_TOP, 90, drop_at_two);
    std::printf("  pair:   solid floor highest=%.3f (the floor is at %.1f)\n",
                on_solid.highest.to_double(), FLOOR_TOP.to_double());
    // Порог — 12 юнитов подъёма, а не полная высота прыжка: нажатие здесь длиной в ОДИН тик, то
    // есть прыжок обрывается отпусканием и поднимает на минимальную высоту профиля (16, замер —
    // 22.4). Требовать полные 64 значило бы проверять длину нажатия вместо того, выдан ли прыжок.
    check(on_solid.highest < FLOOR_TOP - fx(12),
          "pair: 'down + jump' on solid ground is an ordinary jump, not a swallowed press");
}

// Зажатое «вниз» само по себе не роняет: спуск — событие ФРОНТА прыжка, и стоящий персонаж,
// пригнувшийся на платформе, обязан на ней остаться.
void test_holding_down_alone_keeps_standing() {
    const Run r = run(make_level(ONEWAY), PLATFORM_TOP, 60,
                      [](uint32_t) { return Input{false, true}; });
    std::printf("  hold:   feet=%.3f ground=%d lost=%d\n", r.final_feet.to_double(),
                r.ground_at_end ? 1 : 0, r.lost_ground ? 1 : 0);
    check(!r.lost_ground, "holding 'down' without a jump press never loses the platform");
    check(r.final_feet < PLATFORM_TOP + fx(1), "and the feet stay at its top");
}

// После спуска окна coyote нет: спуск — такой же осознанный уход с опоры, как прыжок, и окно после
// него дало бы второй прыжок в воздухе сразу под платформой.
void test_no_coyote_after_a_drop() {
    const Run r = run(make_level(ONEWAY), PLATFORM_TOP, 90,
                      [](uint32_t t) { return Input{t == 2 || t == 4, t < 4}; });
    std::printf("  coyote: highest=%.3f final=%.3f\n", r.highest.to_double(),
                r.final_feet.to_double());
    check(!(r.highest < PLATFORM_TOP - fx(1)), "a jump pressed right after a drop is not granted");
    check(FLOOR_TOP - fx(1) < r.final_feet, "and the fall continues down to the floor");
}

// Спуск с НИЗКОЙ платформы: запомненное нажатие обязано умереть вместе с ним. Иначе «вниз +
// прыжок» роняет персонажа и тут же подбрасывает его с пола — одно нажатие даёт два события.
void test_a_drop_spends_the_buffered_press() {
    // Окно буфера растянуто ОДНИМ полем профиля, и без этого утверждение не наблюдаемо ни на какой
    // раскладке: на штатных шести тиках запомненное нажатие успевает истечь ровно в полёте — замер
    // на сломанной реализации показал буфер, догоревший до нуля на тике посадки. То есть строка,
    // гасящая буфер, выглядела бы мёртвой, а первая же игра с более щедрым окном получила бы два
    // события с одного нажатия. Та же дисциплина, что у гейта склона с выключенным притяжением.
    const Run r = run(make_stacked(), fx(80), 40, drop_at_two, 30);
    std::printf("  buffer: highest=%.3f final=%.3f\n", r.highest.to_double(),
                r.final_feet.to_double());
    // Предпосылка про сам спуск, а утверждение — про то, ГДЕ персонаж остался. Сломанная
    // реализация (буфер доживает) читается по трассе так: посадка на пол на тике 8, прыжок на тике
    // 9 с одного нажатия, и обратно на платформу — итоговые ноги 80 вместо 96. Мерить это высотой
    // подъёма нельзя: обрезанный прыжок поднимает всего на 16, то есть едва выше верха платформы,
    // и порог пришлось бы ставить в сотые доли юнита.
    check(r.lost_ground, "precondition: the drop does leave the platform");
    check(FLOOR_TOP - fx(1) < r.final_feet && r.final_feet < FLOOR_TOP + fx(1),
          "and the press spent on the drop leaves the character on the floor, not back up");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character one-way gate\n");
    test_falls_onto_it();
    test_jumps_through_from_below();
    test_drops_through_on_command();
    test_holding_down_alone_keeps_standing();
    test_no_coyote_after_a_drop();
    test_a_drop_spends_the_buffered_press();
    std::printf("framework-character-oneway: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
