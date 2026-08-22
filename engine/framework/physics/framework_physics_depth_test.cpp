#include <cstdio>

#include "platform_args.hpp"
#include "solver.hpp"
#include "world.hpp"

// Глубина стека, которую держит решатель, — заявленная величина, а не то, что получилось.
//
// До этого гейта она не была записана нигде, и результат виден по истории: `VELOCITY_ITERATIONS`
// меняли, ни разу не спросив, сколько ящиков после этого стоит, а башня из девяти при ШТАТНОМ
// трении заваливалась молча — гейт стека (`framework_physics_stack_test`) прибит к шести и про
// девять не спрашивает.
//
// Утверждений поэтому ДВА на каждое трение, и второе — не украшение:
//   * башня заявленной глубины стоит;
//   * башня на ОДИН ящик глубже всё ещё валится.
// Без второго гейт зелен вакуумно: сцена, переставшая быть башней (ящики разъехались стартовой
// раскладкой, пол уехал, тела перестали быть динамическими), проходит первое утверждение тем
// охотнее, чем сильнее сломана. Второе — позитивный контроль: оно доказывает, что прогон
// действительно нагружает решатель до отказа, и заодно ловит ТИХОЕ УЛУЧШЕНИЕ. Улучшение здесь
// такая же находка, как регресс: за него заплачено временем шага, и цифра в `solver.hpp` обязана
// его назвать, а не молча устареть в меньшую сторону.
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

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
// Десять секунд мало: башня на границе валится не сразу, наклон копится линейно и переходит в
// падение на третьей-четвёртой сотне кадров. Тридцати хватает с запасом на все замеренные случаи.
constexpr uint32_t FRAMES = 30 * 60;

// Заявленная глубина: ящиков, стоящих при трении не ниже `SETTLED_FRICTION`, и при нулевом.
// Числа замерены и обязаны совпадать с таблицей в `solver.hpp`.
constexpr uint32_t DEPTH_WITH_FRICTION = 10;
constexpr uint32_t DEPTH_FRICTIONLESS = 3;
constexpr fix32 SETTLED_FRICTION = fix32::from_float(0.05);

// Пороги «башня ещё башня». Наклон — в оборотах: 512/65536 это чуть меньше трёх градусов, то есть
// заведомо больше дрожания решателя и заведомо меньше заваливания. Снос — полуширина ящика: боком
// на полкорпуса уехал уже не тот стек, который ставили.
constexpr int32_t TILT_LIMIT = 512;
constexpr fix32 DRIFT_LIMIT = HALF;

void build(World& w, fix32 friction, uint32_t boxes) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, friction};
    w.add(floor);

    // Впритык, как и в гейте стека: зазор дал бы падение и удар, нахлёст — стартовое выталкивание.
    for (uint32_t i = 0; i < boxes; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16)};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, friction};
        w.add(b);
    }
}

// Угол приводится к знаковому полуобороту: `set_angle` держит его в [0, 1), и ящик, завалившийся на
// малый отрицательный угол, без приведения читался бы как повёрнутый почти на полный оборот.
int32_t tilt_of(const World& w, uint32_t i) {
    const int32_t raw = w.bodies()[i].angle.raw;
    return raw > 32768 ? raw - 65536 : raw;
}

// Стоит ли башня из `boxes` ящиков. Критерий геометрический — наклон и снос, а не скорость:
// заваливание проходит через долгий участок, где скорости уже ненулевые, но ящик ещё на месте, и
// порог по скорости назвал бы падающую башню стоящей ровно до момента, когда она легла.
bool stands(fix32 friction, uint32_t boxes) {
    World w(boxes + 2);
    w.set_sleep_enabled(true);
    build(w, friction, boxes);
    for (uint32_t frame = 1; frame <= FRAMES; ++frame) w.step(DT);
    for (uint32_t i = 1; i <= boxes; ++i) {
        if (tilt_of(w, i) > TILT_LIMIT || tilt_of(w, i) < -TILT_LIMIT) return false;
        if (!(abs_fix(w.bodies()[i].position.x) < DRIFT_LIMIT)) return false;
    }
    return true;
}

void test_depth(const char* what, fix32 friction, uint32_t depth) {
    const bool at = stands(friction, depth);
    const bool over = stands(friction, depth + 1);
    std::printf("  %s: %u boxes %s, %u boxes %s\n", what, depth, at ? "stand" : "FALL", depth + 1,
                over ? "stand" : "fall");
    check(at, "the stack of the declared depth stands");
    // Формулировка «not yet» намеренная: утверждается не то, что глубже стоять НЕ ДОЛЖНО, а то, что
    // заявленная цифра всё ещё описывает границу. Сдвинулась граница — сдвинуть и цифру.
    check(!over, "and one box deeper does not stand yet, so the declared depth is still the edge");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics depth gate (%u velocity iterations)\n", VELOCITY_ITERATIONS);

    // Чётность — условие попеременного обхода (`solver.hpp`), и проверяется она здесь, потому что
    // именно этот гейт мерит то, что нечётное число сломает: систематический момент, копящий наклон.
    check(VELOCITY_ITERATIONS % 2 == 0, "the iteration count stays even, as the traversal requires");

    test_depth("settled friction", SETTLED_FRICTION, DEPTH_WITH_FRICTION);
    // Нулевое трение — законный материал, а не вырожденный вход: гасить остаток ему нечем, поэтому
    // граница своя и вчетверо ниже. Отдельным случаем, чтобы регресс в ней не прятался за первым.
    test_depth("frictionless", fix32{}, DEPTH_FRICTIONLESS);

    std::printf("framework-physics-depth: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
