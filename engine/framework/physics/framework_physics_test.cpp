#include <cstdio>
#include <vector>

#include "framework_physics_scene.hpp"
#include "narrowphase.hpp"
#include "platform_args.hpp"

// Гейт 1 спеки #15: состояние физики после фиксированного числа шагов даёт один и тот же хеш на
// macOS, Linux и Windows — эталон ниже проверяется в CI на каждой из трёх ОС.
//
// Один хеш гейтом быть не может: он ловит расхождение между платформами, но про сцену, где всё
// провалилось сквозь пол одинаково на всех трёх, скажет «зелено». Поэтому рядом с ним —
// наблюдаемые утверждения о поведении, каждое из которых падает на своей поломке: тела над полом
// (иначе туннелирование), стопка успокоилась (иначе накачка энергии решателем), трение съело
// горизонтальный ход (иначе касательный импульс не работает), формы сталкиваются по геометрии.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::physics;

const Body* find(const World& w, uint32_t key) {
    for (const Body& b : w.bodies()) {
        if (b.key == key) return &b;
    }
    return nullptr;
}

void test_shapes() {
    BodyDesc a;
    a.key = 1;
    a.shape = circle(fix32::from_int(10));
    a.position = {fix32{}, fix32{}};
    BodyDesc b = a;
    b.key = 2;
    b.position = {fix32::from_int(15), fix32{}};

    Contact c;
    check(collide(make_body(a), make_body(b), c), "circle-circle overlap detected");
    check(c.penetration == fix32::from_int(5), "circle-circle penetration = 5");
    check(c.normal == Vec2{fix32::from_int(1), fix32{}}, "circle-circle normal points a->b");

    b.position = {fix32::from_int(21), fix32{}};
    check(!collide(make_body(a), make_body(b), c), "circle-circle apart -> no contact");

    // Совпавшие центры — вырожденный вход, у которого обязан быть ОПРЕДЕЛЁННЫЙ ответ: без него
    // два тела в одной точке разъезжаются как придётся, и golden гуляет от запуска к запуску.
    b.position = a.position;
    check(collide(make_body(a), make_body(b), c), "coincident centres still collide");
    check(c.normal == Vec2{fix32{}, fix32::from_int(-1)}, "coincident centres -> fixed normal");

    BodyDesc plate;
    plate.key = 3;
    plate.type = BodyType::Static;
    plate.shape = box(fix32::from_int(50), fix32::from_int(10));
    plate.position = {fix32{}, fix32::from_int(15)};
    check(collide(make_body(a), make_body(plate), c), "circle-box overlap detected");
    check(c.normal == Vec2{fix32{}, fix32::from_int(1)}, "circle-box normal points down to plate");

    // Коробка против круга обязана дать ЗЕРКАЛЬНУЮ нормаль той же величины: это не косметика,
    // а проверка того, что перестановка тел проходит через ту же геометрию, а не через вторую
    // реализацию, которая однажды разойдётся с первой на округлении.
    Contact flipped;
    check(collide(make_body(plate), make_body(a), flipped), "box-circle overlap detected");
    check(flipped.normal == -c.normal, "box-circle normal is mirrored");
    check(flipped.penetration == c.penetration, "box-circle penetration matches");
}

void test_rest() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    fixture::run(w, fixture::STEPS);

    const fix32 floor_top = fix32::from_int(192);
    const fix32 quiet = fix32::from_int(2);
    fix32 lowest_bottom = -WORLD_HALF;
    for (const Body& b : w.bodies()) {
        if (b.type != BodyType::Dynamic) continue;
        const fix32 half_h = b.shape.kind == ShapeKind::Circle ? b.shape.radius : b.shape.half.y;
        // Низ тела не ушёл заметно ниже верха пола: провал глубже допуска — это туннелирование
        // или решатель, не удержавший контакт, и то и другое обязано быть красным.
        check(b.position.y + half_h < floor_top + fix32::from_int(2), "body rests on the floor");
        check(abs_fix(b.velocity.y) < quiet, "vertical motion settled");
        check(abs_fix(b.position.x) < fix32::from_int(248), "body stayed between the walls");
        // Упало, а не зависло: стартовые высоты сцены отрицательные, пол — на +200. Без этой
        // строки «покоится» одинаково подписывалось бы под телом, застывшим в воздухе, — а
        // застывшее тело и есть самый частый вид сломанной интеграции.
        check(fix32::from_int(0) < b.position.y, "body actually fell toward the floor");
        lowest_bottom = max_fix(lowest_bottom, b.position.y + half_h);
    }
    // И хотя бы одно из них лежит НА полу, а не в стопке над ним: иначе вся куча могла зависнуть
    // на первом же контакте и всё равно пройти проверки выше.
    check(floor_top - fix32::from_int(2) < lowest_bottom, "the stack reaches the floor");

    // Трение: ящики стартовали с ходом 40 юнит/с вбок, лёгшая на шершавый пол стопка обязана
    // остановиться. Без касательного импульса они скользили бы вечно — гравитация им не мешает.
    for (uint32_t key = 10; key < 15; ++key) {
        const Body* b = find(w, key);
        check(b != nullptr && abs_fix(b->velocity.x) < quiet, "friction stopped the box");
    }
}

void test_static_immobile() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    const Vec2 floor_before = w.bodies()[0].position;
    fixture::run(w, fixture::STEPS);
    check(w.bodies()[0].position == floor_before, "static floor never moves");
}

// Эталон гейта 1. Снят с прогона, в котором ПРОШЛИ все проверки поведения выше, — и это
// единственное, что отличает эталон от закреплённого дефекта: хеш сцены, где всё провалилось
// сквозь пол, выглядит ровно так же убедительно. Меняется только вместе с осознанной сменой
// физики — числа итераций, порядка решения, формул. Разошёлся сам по себе — это находка, а не
// повод перезаписать константу.
constexpr uint64_t GOLDEN = 0x3977752decf95c28ULL;

void test_golden() {
    World w(fixture::CAPACITY);
    std::vector<BodyDesc> descs;
    fixture::describe(descs);
    fixture::fill(w, descs);
    fixture::run(w, fixture::STEPS);
    const uint64_t h = w.hash();
    std::printf("  physics-state-hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "physics state hash matches the golden");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics gate\n");
    test_shapes();
    test_rest();
    test_static_immobile();
    test_golden();
    std::printf("framework-physics: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
