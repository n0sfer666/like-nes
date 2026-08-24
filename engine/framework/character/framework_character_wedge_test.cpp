#include <cstdio>

#include "../tilemap/query.hpp"
#include "controller.hpp"
#include "platform_args.hpp"

// ПИН ИЗВЕСТНОГО ДЕФЕКТА, а не гейт исправного поведения (решение владельца 2026-08-24).
//
// Сбежавший по склону в 45° персонаж, у которого стена стоит РОВНО в ширину корпуса от подножия,
// теряет опору и залипает в воздухе НАВСЕГДА: позиция перестаёт меняться, `on_ground` ложь, падения
// нет, и отпускание ввода не помогает. Найдено трассой образца-платформера, где подножие холма
// стояло на x=352, а стена ступени — на x=368 при корпусе шириной 16.
//
// Утверждения здесь ПЕРЕВЁРНУТЫ нарочно: гейт держит зелёным то, что дефект ВОСПРОИЗВОДИТСЯ. Так
// он стоит уликой, а не строчкой в dev-log, и в день, когда контроллер починят, ЭТА цель покраснеет
// и потребует перевернуть утверждения обратно — молча пережить починку пин не может. Обычный гейт
// «клина нет» тут не годится: он красный с рождения, а красный гейт в дереве через неделю читается
// как «у нас всегда так» и перестаёт что-либо значить.
//
// Один пин ничего не доказывает без ПАРЫ: сцена, отличающаяся ровно положением стены, обязана
// пройти клин здоровым. Без неё «залип» было бы правдой и про сборку, где залипает всё подряд.
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
constexpr fix32 HALF_W = fix32::from_int(8);
constexpr fix32 HALF_H = fix32::from_int(16);

constexpr tilemap::TileFlags SLOPE_DOWN =
    tilemap::TILE_SOLID | tilemap::TILE_SLOPE | tilemap::TILE_SLOPE_FLIP_X;

// Корпус РОВНО в тайл шириной — как у образца-платформера, а не уже, как в гейте склона. Именно
// совпадение ширины корпуса с зазором и есть предмет: сузив корпус, клин перестанут воспроизводить.
CharacterHull make_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(HALF_W, HALF_H));
    return h;
}

// Терраса на y=64 слева, склон вниз в колонке 5, пол на y=80, стена — колонкой `wall_col`.
// Подножие склона на x=96, поэтому `wall_col=7` даёт зазор ровно в 16 юнитов: ширину корпуса.
tilemap::TileGrid make_wedge(uint32_t wall_col) {
    tilemap::TileGrid g({fix32{}, fix32{}}, TILE, 12, 8);
    g.fill(0, 5, 12, 8, tilemap::TILE_SOLID);
    g.fill(0, 4, 5, 5, tilemap::TILE_SOLID);
    g.set(5, 4, SLOPE_DOWN);
    g.fill(wall_col, 0, wall_col + 1, 5, tilemap::TILE_SOLID);
    return g;
}

struct Run {
    Vec2 last;
    Vec2 midway;           // где он был в тик отпускания ввода
    bool on_ground;
    bool hit_wall;
    uint32_t airborne_tail; // сколько ПОСЛЕДНИХ тиков подряд он был без опоры
};

fix32 dist(Vec2 a, Vec2 b) {
    const fix32 dx = a.x - b.x;
    const fix32 dy = a.y - b.y;
    return abs_fix(dx) + abs_fix(dy);
}

// Сбежать по склону вправо и упереться в стену. Ввод отпускается на середине прогона: залипший
// персонаж обязан остаться залипшим и БЕЗ нажатой кнопки — иначе «не двигается» описывало бы
// отпущенный ввод, а не потерянную опору.
Run run_into(uint32_t wall_col, uint32_t ticks) {
    const tilemap::TileGrid g = make_wedge(wall_col);
    const MoveProfile p = sanitize(default_profile());
    const MoveDerived d = derive(p, tick_dt());
    const CollisionScene s{nullptr, &g};
    const CharacterHull h = make_hull();

    Character c;
    c.position = {fx(20), fx(64) - HALF_H - SKIN};
    c.on_ground = true;
    c.state = MoveState::Ground;

    Run r{c.position, c.position, true, false, 0};
    for (uint32_t t = 0; t < ticks; ++t) {
        MoveInput in;
        if (t < ticks / 2) in.move_x = fx(1);
        step(s, h, p, d, in, tick_dt(), c);
        r.hit_wall = r.hit_wall || c.hit_wall;
        r.airborne_tail = c.on_ground ? 0 : r.airborne_tail + 1;
        if (t + 1 == ticks / 2) r.midway = c.position;
    }
    r.last = c.position;
    r.on_ground = c.on_ground;
    return r;
}

// Стена в ширину корпуса от подножия: дефект. Числа в утверждениях — не «побольше»: сто тиков без
// опоры это 1.6 секунды свободного падения, за которые персонаж уехал бы вниз на сотни юнитов.
// Именно НОЛЬ пройденного при отсутствующей опоре и есть улика: не «застрял у стены», а «висит».
void test_the_wedge_still_traps_him() {
    const Run r = run_into(7, 200);
    std::printf("  wedge(16): pos %.3f %.3f ground=%d wall=%d airborne_tail=%u drift=%.3f\n",
                r.last.x.to_double(), r.last.y.to_double(), r.on_ground ? 1 : 0,
                r.hit_wall ? 1 : 0, r.airborne_tail, dist(r.last, r.midway).to_double());
    // Предпосылка: прогон вообще доехал до стены. Без неё «висит в воздухе» было бы правдой и про
    // персонажа, свалившегося со склона в чистом поле.
    check(r.hit_wall, "precondition: the run does reach the wall");
    check(!r.on_ground, "KNOWN DEFECT: the hero loses the ground at the slope base");
    check(90 < r.airborne_tail, "KNOWN DEFECT: and never gets it back, input released or not");
    check(dist(r.last, r.midway) < fx(1),
          "KNOWN DEFECT: yet he does not fall either - a hundred ticks, nowhere");
}

// Та же сцена, стена на тайл дальше: клина нет. Пара, без которой пин выше не значит ничего.
void test_a_wider_gap_is_healthy() {
    const Run r = run_into(8, 200);
    std::printf("  pair (32): pos %.3f %.3f ground=%d wall=%d airborne_tail=%u\n",
                r.last.x.to_double(), r.last.y.to_double(), r.on_ground ? 1 : 0,
                r.hit_wall ? 1 : 0, r.airborne_tail);
    check(r.hit_wall, "precondition: the paired run reaches its wall too");
    check(r.on_ground, "pair: with the wall a tile further the descent keeps the ground");
    check(r.airborne_tail == 0, "pair: and nothing hangs in the air");
    // И стоит он на НИЖНЕМ полу (y=80, центр 63.875), а не на террасе, с которой начал (центр
    // 47.875): без этого «опора не терялась» было бы правдой и про персонажа, не съехавшего со
    // склона вовсе. Порог между двумя уровнями, а не «около 63.875»: полутик посадки не предмет.
    check(fx(60) < r.last.y, "pair: and he is standing on the lower floor, not still on the terrace");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character wedge pin (KNOWN DEFECT: slope base one hull width from a wall)\n");
    test_the_wedge_still_traps_him();
    test_a_wider_gap_is_healthy();
    if (fails == 0)
        std::printf("  the defect is still present: fix it, then invert the assertions above\n");
    std::printf("framework-character-wedge: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
