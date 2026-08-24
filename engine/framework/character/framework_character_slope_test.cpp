#include <cstdio>

#include "../tilemap/query.hpp"
#include "controller.hpp"
#include "platform_args.hpp"

// Гейт ХОДЬБЫ ПО СКЛОНУ (вертикаль 3, шаг B): тайл-склон в 45° — это пол, по нему поднимаются и с
// него спускаются, не теряя опоры; тот же склон при другом пороге профиля — стена.
//
// Своей целью и своей раскладкой по той же причине, что у гейтов прощения: голден траектории идёт
// по сцене БЕЗ единого склона, то есть про склоны не утверждает ничего, а вопрос здесь мерится не
// хешем, а высотой и признаком опоры.
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

// Хитбокс УЖЕ тайла нарочно: шириной в тайл он на стыке «склон + плита» задевал бы обе грани сразу,
// и потерянная опора читалась бы как дефект склона, хотя пришла бы от соседа.
CharacterHull make_hull() {
    CharacterHull h;
    h.shape = physics::sanitize(physics::box(HALF_W, HALF_H));
    return h;
}

constexpr tilemap::TileFlags SLOPE_UP = tilemap::TILE_SOLID | tilemap::TILE_SLOPE;
constexpr tilemap::TileFlags SLOPE_DOWN =
    tilemap::TILE_SOLID | tilemap::TILE_SLOPE | tilemap::TILE_SLOPE_FLIP_X;

// Подъём: ровная площадка на y = 80, один склон в колонке 5 и площадка на y = 64 за ним. Ступенька
// ровно в тайл — то есть склон покрывает ВЕСЬ перепад, и «поднялся на 16» означает «прошёл склон
// целиком», а не «зацепился за угол».
tilemap::TileGrid make_ramp_up() {
    tilemap::TileGrid g({fix32{}, fix32{}}, TILE, 12, 8);
    g.fill(0, 5, 12, 8, tilemap::TILE_SOLID);
    g.set(5, 4, SLOPE_UP);
    g.fill(6, 4, 12, 5, tilemap::TILE_SOLID);
    return g;
}

// Спуск — та же ступенька зеркально: площадка на y = 64 слева, склон в колонке 6, низ на y = 80.
tilemap::TileGrid make_ramp_down() {
    tilemap::TileGrid g({fix32{}, fix32{}}, TILE, 12, 8);
    g.fill(0, 5, 12, 8, tilemap::TILE_SOLID);
    g.fill(0, 4, 6, 5, tilemap::TILE_SOLID);
    g.set(6, 4, SLOPE_DOWN);
    return g;
}

struct Run {
    fix32 climb;        // сколько персонаж набрал по высоте: вверх положительно
    fix32 peak;         // САМАЯ высокая точка прогона, а не только его конец
    fix32 travelled;    // сколько прошёл вправо
    bool kept_ground;   // опора не терялась НИ НА ОДИН тик
    bool hit_wall;      // склон хоть раз ответил как стена
};

// Пройти вправо `ticks` тиков с заданным порогом проходимости. Ввод один и тот же во всех прогонах:
// расходиться они обязаны порогом и раскладкой, а не силой нажатия.
Run walk(const tilemap::TileGrid& g, fix32 max_slope, fix32 start_x, fix32 feet_y, uint32_t ticks,
         fix32 ground_snap = default_profile().ground_snap) {
    MoveProfile p = default_profile();
    p.max_slope = max_slope;
    p.ground_snap = ground_snap;
    p = sanitize(p);
    const MoveDerived d = derive(p, tick_dt());
    const CollisionScene s{nullptr, &g};
    Character c;
    c.position = {start_x, feet_y - HALF_H - SKIN};
    c.on_ground = true;
    c.state = MoveState::Ground;
    const Vec2 from = c.position;
    Run r{fix32{}, fix32{}, fix32{}, true, false};
    for (uint32_t t = 0; t < ticks; ++t) {
        MoveInput in;
        in.move_x = fix32::from_int(1);
        step(s, make_hull(), p, d, in, tick_dt(), c);
        r.peak = max_fix(r.peak, from.y - c.position.y);
        r.kept_ground = r.kept_ground && c.on_ground;
        r.hit_wall = r.hit_wall || c.hit_wall;
    }
    r.climb = from.y - c.position.y;
    r.travelled = c.position.x - from.x;
    return r;
}

void test_walking_up() {
    const tilemap::TileGrid g = make_ramp_up();
    const Run up = walk(g, default_profile().max_slope, fx(40), fx(80), 20);
    std::printf("  up:   climb=%.3f travelled=%.3f ground=%d wall=%d\n", up.climb.to_double(),
                up.travelled.to_double(), up.kept_ground ? 1 : 0, up.hit_wall ? 1 : 0);
    // Предпосылка: прогон вообще дошёл до склона и прошёл его. Без неё «опора не терялась» было бы
    // правдой и про персонажа, простоявшего сорок тиков на ровной площадке.
    check(fx(56) < up.travelled, "precondition: the walk actually crosses the ramp");
    // Перепад ровно тайл, и требуется он ЦЕЛИКОМ: подъём на половину означал бы, что персонаж встал
    // на склоне, а признак опоры при этом остался бы честным.
    check(fx(15) < up.climb && up.climb < fx(17), "walking right up a 45-degree tile gains a tile");
    check(up.kept_ground, "and the slope is ground on every tick of the climb");
    check(!up.hit_wall, "a walkable slope is never reported as a wall");
}

void test_walking_down() {
    const tilemap::TileGrid g = make_ramp_down();
    const Run down = walk(g, default_profile().max_slope, fx(40), fx(64), 20);
    std::printf("  down: climb=%.3f travelled=%.3f ground=%d wall=%d\n", down.climb.to_double(),
                down.travelled.to_double(), down.kept_ground ? 1 : 0, down.hit_wall ? 1 : 0);
    check(fx(56) < down.travelled, "precondition: the walk actually crosses the ramp");
    check(down.climb < fx(-15) && fx(-17) < down.climb, "walking right down it loses a tile");
    // Спуск держится ПРИТЯЖЕНИЕМ: за тик персонаж уходит вперёд на 5.67 юнита, то есть отрывается
    // от склона на столько же, и без окна он ехал бы вниз прыжками, теряя опору каждый тик.
    check(down.kept_ground, "and the ground snap keeps the descent in contact with the slope");
}

// Подброс. ТА ЖЕ раскладка и тот же ввод, что в подъёме, — профиль отличается ОДНИМ полем: окно
// притяжения к полу выключено нулём. Пара нужна затем, что на штатном профиле подброса не видно
// вовсе, и «не подбрасывает» читалось бы как свойство склона: сошедшего с верхнего края персонажа
// подхватывает притяжение и гасит ему вертикаль тем же тиком. Выключив окно, мы видим голую
// скорость ВДОЛЬ грани — и то, что снимает её именно сход с опоры. Замер на сломанной реализации:
// без гашения тот же прогон даёт пик 35.7, то есть больше тайла сверх верха склона.
void test_the_ramp_is_not_a_jump() {
    const tilemap::TileGrid g = make_ramp_up();
    const Run r = walk(g, default_profile().max_slope, fx(40), fx(80), 20, fix32{});
    std::printf("  launch: peak=%.3f (the ramp top is 16.0 above the start)\n", r.peak.to_double());
    // Предпосылка отделяет «не подбросило» от «не дошёл до склона»: без неё утверждение ниже
    // проходило бы и у персонажа, упёршегося в первый же тайл.
    check(fx(15) < r.peak, "precondition: the walk does climb the ramp");
    // Верх склона — ровно тайл над стартом, и полтора юнита сверху это перелёт последнего тика по
    // склону. Всё, что выше, — уже скорость вдоль грани, пережившая сход с неё.
    check(r.peak < fx(18), "and does not keep the slope's rise once the slope ends");
}

void test_the_threshold_decides() {
    const tilemap::TileGrid g = make_ramp_up();
    const Run strict = walk(g, fix32{}, fx(40), fx(80), 20);
    std::printf("  strict: climb=%.3f ground=%d wall=%d\n", strict.climb.to_double(),
                strict.kept_ground ? 1 : 0, strict.hit_wall ? 1 : 0);
    // ТА ЖЕ раскладка и тот же ввод, что в подъёме, — отличается один порог. Без этой пары
    // утверждения выше доказывали бы лишь то, что склон вообще отвечает свипу, а обязаны
    // доказывать, что «пол это или стена» решает ПРОФИЛЬ.
    check(!strict.kept_ground, "pair: at threshold zero the same slope stops being ground");
    check(strict.hit_wall, "pair: and the tick reports it as a wall");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character slope gate\n");
    test_walking_up();
    test_walking_down();
    test_the_ramp_is_not_a_jump();
    test_the_threshold_decides();
    std::printf("framework-character-slope: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
