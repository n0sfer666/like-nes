#include <cstdio>

#include "framework_character_scene.hpp"
#include "platform_args.hpp"
#include "profile.hpp"

// Шов смешанной сцены (`collision.hpp`, вертикаль 3 спеки #16): два представления геометрии — один
// ответ. Гейт отдельной целью от голдена по той же причине, что и гейты профиля: имя упавшей цели
// в логе CI обязано называть класс поломки, а «траектория разошлась» про порядок слияния молчит.
//
// Проверяется ПОРЯДОК, а не наличие ответа, и наблюдаем он там, где источники СПОРЯТ: каждая сцена
// ниже держит обоих, и каждый ответ сверяется с тем, что тот же источник даёт В ОДИНОЧКУ — слияние
// обязано выбирать из двух готовых ответов, а не сочинять третий.
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

fix32 fx(int v) { return fix32::from_int(v); }

const Vec2 DOWN{fix32{}, fx(200)};

// Пол под ногами, а не под серединой корпуса: свип из начала координат при поле на нуле стартует
// уже ВНУТРИ пола, доля выходит нулевой, и «источник ответил» читается как «источник промолчал».
constexpr fix32 PLAIN_FLOOR_TOP = fix32::from_int(100);

// Сцена равной доли: путь (200, 100), то есть стена закрывает зазор вдвое быстрее пола, значит
// вдвое больше обязан быть и сам зазор. Оба свипа встают на КАСАНИИ, без вычета допуска
// проникновения: прогон на `+ CONTACT_SLOP` в зазоре давал 0.500610 против 0.500305. Обе доли
// выходят ровно 1/2 и представимы в Q16.16 точно — равенство долей здесь предусловие случая, и
// округление до кванта превратило бы его в правило 1.
constexpr fix32 EQUAL_GAP = fix32::from_int(100);
constexpr fix32 TIE_GAP = fix32::from_int(50);
constexpr fix32 EQUAL_WALL_LEFT = HULL_HALF_W + EQUAL_GAP;
constexpr fix32 EQUAL_FLOOR_TOP = HULL_HALF_H + EQUAL_GAP / fix32::from_int(2);

// Тела: верхняя грань пола ровно на `top`, левая грань стены ровно на `left`.
physics::World body_floor(fix32 top) {
    physics::World w(4);
    w.set_gravity({fix32{}, fix32{}});
    add_static(w, 1, {fix32{}, top + fx(100)}, fx(500), fx(100));
    return w;
}

physics::World body_wall(fix32 left) {
    physics::World w(4);
    w.set_gravity({fix32{}, fix32{}});
    add_static(w, 1, {left + fx(100), fix32{}}, fx(100), fx(500));
    return w;
}

tilemap::TileGrid tile_floor(fix32 top) {
    tilemap::TileGrid g({fx(-500), top}, fx(10), 100, 10);
    g.fill(0, 0, 100, 10, tilemap::TILE_SOLID);
    return g;
}

tilemap::TileGrid tile_wall(fix32 left) {
    tilemap::TileGrid g({left, fx(-500)}, fx(10), 10, 100);
    g.fill(0, 0, 10, 100, tilemap::TILE_SOLID);
    return g;
}

fix32 alone(const CollisionScene& s, const CharacterHull& hull, Vec2 travel, Vec2& normal) {
    SceneHit h;
    if (!cast_nearest(s, hull, {fix32{}, fix32{}}, travel, h)) return fx(-1);
    normal = h.normal;
    return h.fraction;
}

// Пустая сцена законна и отвечает «пути ничто не мешает»; каждый источник в одиночку отвечает сам
// за себя; оба вместе дают ТОТ ЖЕ ответ, а не третий. Без последнего слияние, всегда возвращающее
// долю ноль, прошло бы три случая из четырёх.
void test_sources_alone_and_together() {
    const CharacterHull hull = make_hull();
    const physics::World w = body_floor(PLAIN_FLOOR_TOP);
    const tilemap::TileGrid g = tile_floor(PLAIN_FLOOR_TOP);
    Vec2 n{};

    SceneHit h;
    check(!cast_nearest({nullptr, nullptr}, hull, {fix32{}, fix32{}}, DOWN, h),
          "an empty scene blocks nothing");

    const fix32 body_only = alone({&w, nullptr}, hull, DOWN, n);
    check(fix32{} < body_only && n == Vec2{fix32{}, fx(-1)}, "the body floor alone answers");
    const fix32 tiles_only = alone({nullptr, &g}, hull, DOWN, n);
    check(fix32{} < tiles_only && n == Vec2{fix32{}, fx(-1)}, "the tiled floor alone answers");
    check(body_only == tiles_only, "control: the two floors are the same floor");

    const fix32 merged = alone({&w, &g}, hull, DOWN, n);
    check(merged == body_only, "and the mixed scene answers neither more nor less");
}

// Правило 1: побеждает МЕНЬШАЯ доля, чей бы источник это ни был. Оба направления обязательны:
// слияние, всегда отдающее ответ тайлов, прошло бы одну половину.
void test_nearer_wins() {
    const CharacterHull hull = make_hull();
    Vec2 n{};
    {
        const physics::World w = body_floor(fx(80));
        const tilemap::TileGrid g = tile_floor(fx(40));
        const fix32 far_ = alone({&w, nullptr}, hull, DOWN, n);
        const fix32 near_ = alone({nullptr, &g}, hull, DOWN, n);
        check(near_ < far_, "control: the tiled floor really is the nearer one");
        check(alone({&w, &g}, hull, DOWN, n) == near_, "the nearer tiles win over the farther body");
    }
    {
        const physics::World w = body_floor(fx(40));
        const tilemap::TileGrid g = tile_floor(fx(80));
        const fix32 near_ = alone({&w, nullptr}, hull, DOWN, n);
        const fix32 far_ = alone({nullptr, &g}, hull, DOWN, n);
        check(near_ < far_, "control: now the body is the nearer one");
        check(alone({&w, &g}, hull, DOWN, n) == near_, "and the nearer body wins over the tiles");
    }
}

// Правило 2: на РАВНОЙ доле выигрывает более встречная поверхность. Путь наклонён нарочно — под 45
// градусов стена и пол одинаково встречны, и правило было бы ненаблюдаемо. Равенство долей не
// предполагается, а УТВЕРЖДАЕТСЯ: разъехавшись на квант, случай молча стал бы правилом 1.
void test_equal_fraction_prefers_the_head_on_face() {
    const CharacterHull hull = make_hull();
    const Vec2 travel{fx(200), fx(100)};
    const physics::World w = body_floor(EQUAL_FLOOR_TOP);
    const tilemap::TileGrid g = tile_wall(EQUAL_WALL_LEFT);
    Vec2 body_n{}, tile_n{};
    const fix32 body_only = alone({&w, nullptr}, hull, travel, body_n);
    const fix32 tiles_only = alone({nullptr, &g}, hull, travel, tile_n);
    std::printf("  equal-fraction scene: body=%.6f tiles=%.6f\n", body_only.to_double(),
                tiles_only.to_double());
    check(body_only == tiles_only, "control: both faces are reached at the very same fraction");

    Vec2 n{};
    check(alone({&w, &g}, hull, travel, n) == body_only, "the merge keeps that fraction");
    check(n == Vec2{fx(-1), fix32{}}, "and answers with the face the character drives into");
}

// Правило 3: на равной доле И равной встречности отвечают ТЕЛА. Оно произвольное, поэтому берутся
// ОБА расклада — тайлы-стена с телом-полом и наоборот: слияние, отвечающее по нормали, а не по
// источнику, прошло бы один из них.
void test_full_tie_goes_to_bodies() {
    const CharacterHull hull = make_hull();
    const Vec2 travel{fx(100), fx(100)};
    // Путь под 45 градусов: зазоры равны, значит равны и доли, и равна встречность обеих граней —
    // правила 1 и 2 промолчали, отвечать остаётся правилу 3.
    Vec2 n{};
    {
        const physics::World w = body_floor(HULL_HALF_H + TIE_GAP);
        const tilemap::TileGrid g = tile_wall(HULL_HALF_W + TIE_GAP);
        const fix32 body_only = alone({&w, nullptr}, hull, travel, n);
        const fix32 tiles_only = alone({nullptr, &g}, hull, travel, n);
        check(body_only == tiles_only, "control: the tie is a real tie");
        check(alone({&w, &g}, hull, travel, n) == body_only && n == Vec2{fix32{}, fx(-1)},
              "the body floor takes the tie");
    }
    {
        const physics::World w = body_wall(HULL_HALF_W + TIE_GAP);
        const tilemap::TileGrid g = tile_floor(HULL_HALF_H + TIE_GAP);
        const fix32 body_only = alone({&w, nullptr}, hull, travel, n);
        const fix32 tiles_only = alone({nullptr, &g}, hull, travel, n);
        check(body_only == tiles_only, "control: and so is the mirrored tie");
        check(alone({&w, &g}, hull, travel, n) == body_only && n == Vec2{fx(-1), fix32{}},
              "the body wall takes it too: the rule names the source, not the normal");
    }
}

// Проба опоры ходит тем же слиянием: пол под персонажем обязан быть опорой из обоих источников.
// Контроль — стена: свипу вниз она отвечает, а опорой не является.
void test_ground_probe_sees_both() {
    const CharacterHull hull = make_hull();
    const Vec2 rest{fix32{}, -HULL_HALF_H - SKIN};
    const physics::World w = body_floor(fix32{});
    const tilemap::TileGrid g = tile_floor(fix32{});
    const fix32 walkable = default_profile().max_slope;
    check(probe_ground({&w, nullptr}, hull, rest, walkable), "a body floor is ground");
    check(probe_ground({nullptr, &g}, hull, rest, walkable), "a tiled floor is ground");
    check(probe_ground({&w, &g}, hull, rest, walkable), "and so is a mixed one");

    const tilemap::TileGrid wall = tile_wall(HULL_HALF_W + SKIN);
    check(!probe_ground({nullptr, &wall}, hull, {fix32{}, fix32{}}, walkable),
          "control: a wall is not ground");
}
} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework character collision-scene gate\n");
    test_sources_alone_and_together();
    test_nearer_wins();
    test_equal_fraction_prefers_the_head_on_face();
    test_full_tie_goes_to_bodies();
    test_ground_probe_sees_both();
    std::printf("framework-character-collision: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
