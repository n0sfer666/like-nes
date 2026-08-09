#include <cstdio>

#include "fixtrig.hpp"
#include "platform_args.hpp"

// Свойства целочисленной тригонометрии. Проверяются именно СВОЙСТВА, а не значения из чужого
// синуса: сверка с `std::sin` означала бы, что эталон берётся у той самой libm, расхождение
// которой между платформами и есть причина существования таблицы.
//
// Каждое утверждение здесь — то, на что опирается физика: период (тело крутится десятки секунд),
// нечётность (поворот против часовой обязан быть зеркалом поворота по часовой), единичность
// (s^2 + c^2 != 1 растягивает форму при повороте), монотонность (ступенька в таблице выглядит как
// рывок тела на ровном ходу).
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

constexpr int32_t ONE = fix32::ONE;
constexpr int32_t QUARTER_TURN = ONE / 4;

// Шаг обхода — один raw, то есть 1/65536 оборота: обходятся ВСЕ представимые углы одного оборота,
// а не выборка из них. Ошибка интерполяции максимальна между узлами таблицы, и сетка «по узлам»
// прошла бы мимо неё целиком.
constexpr int32_t STEP = 1;

fix32 turns(int32_t raw) { return fix32::from_raw(raw); }

int64_t sq(int64_t v) { return v * v; }

void test_exact_nodes() {
    using framework::cos_turns;
    using framework::sin_turns;
    check(sin_turns(turns(0)).raw == 0, "sin(0) == 0");
    check(sin_turns(turns(QUARTER_TURN)).raw == ONE, "sin(1/4 turn) == 1");
    check(sin_turns(turns(2 * QUARTER_TURN)).raw == 0, "sin(1/2 turn) == 0");
    check(sin_turns(turns(3 * QUARTER_TURN)).raw == -ONE, "sin(3/4 turn) == -1");
    check(cos_turns(turns(0)).raw == ONE, "cos(0) == 1");
    check(cos_turns(turns(QUARTER_TURN)).raw == 0, "cos(1/4 turn) == 0");
    check(cos_turns(turns(2 * QUARTER_TURN)).raw == -ONE, "cos(1/2 turn) == -1");
    check(framework::turns_from_degrees(90).raw == QUARTER_TURN, "90 degrees == 1/4 turn");
    check(framework::turns_from_degrees(-45).raw == -QUARTER_TURN / 2, "-45 degrees is exact");
}

void test_symmetry_and_period() {
    using framework::cos_turns;
    using framework::sin_turns;
    bool odd = true, periodic = true, phase = true;
    for (int32_t a = 0; a < ONE; a += STEP) {
        const fix32 s = sin_turns(turns(a));
        // Период равен единице ровно, поэтому совпадение обязано быть побитовым: угол,
        // накопленный за десять оборотов, — это тот же угол, а не «примерно тот же».
        if (sin_turns(turns(a + ONE)).raw != s.raw) periodic = false;
        if (sin_turns(turns(-a)).raw != -s.raw) odd = false;
        if (cos_turns(turns(a)).raw != sin_turns(turns(a + QUARTER_TURN)).raw) phase = false;
    }
    check(periodic, "the period is one turn, bit for bit");
    check(odd, "sin(-a) == -sin(a) bit for bit");
    check(phase, "cos(a) == sin(a + 1/4 turn)");
}

// Допуск на единичность — в raw-единицах Q16.16 суммы квадратов. Хорда таблицы проходит НИЖЕ
// синуса (он вогнут на четверти), к этому добавляется округление интерполяции, и в сумму квадратов
// обе попадают удвоенными: d(s^2 + c^2) = 2*s*ds + 2*c*dc. Замер печатается ниже и держится на 3 —
// допуск взят на единицу шире наблюдаемого, а не «с запасом»: широкий перестал бы отличать
// интерполяцию от испорченного узла таблицы.
constexpr int64_t UNIT_TOLERANCE = 4;

void test_unit_circle() {
    int64_t worst = 0;
    for (int32_t a = 0; a < ONE; a += STEP) {
        const int64_t s = framework::sin_turns(turns(a)).raw;
        const int64_t c = framework::cos_turns(turns(a)).raw;
        const int64_t err = ((sq(s) + sq(c)) >> fix32::SHIFT) - ONE;
        const int64_t mag = err < 0 ? -err : err;
        if (mag > worst) worst = mag;
    }
    std::printf("  unit circle: worst |s^2 + c^2 - 1| = %lld raw\n",
                static_cast<long long>(worst));
    check(worst <= UNIT_TOLERANCE, "s^2 + c^2 == 1 within the interpolation tolerance");
}

void test_monotonic_quarter() {
    bool ok = true;
    int32_t previous = framework::sin_turns(turns(0)).raw;
    for (int32_t a = STEP; a <= QUARTER_TURN; a += STEP) {
        const int32_t value = framework::sin_turns(turns(a)).raw;
        // Строгий рост требовать нельзя: у пика соседние углы дают одно значение по округлению.
        // А вот падение означало бы перепутанный узел таблицы — ровно то, что ищется.
        if (value < previous) ok = false;
        previous = value;
    }
    check(ok, "sine never decreases on the first quarter");
}

// Допуск на длину повёрнутого вектора: ошибка синуса входит в обе координаты, и на векторе длиной
// 1000 юнитов это сотые доли юнита. Замер печатается — тот же принцип, что и с единичностью:
// на векторе длиной 1000 наблюдается 1254 raw (0.019 юнита) на самом повороте и 2507 на обратном
// ходе, где округлений вдвое больше; разрешено 4096 — полшага сверх худшего замера.
constexpr int32_t LENGTH_TOLERANCE = ONE / 16;

void test_rotate() {
    using framework::rotate;
    using framework::rotation;
    using framework::Vec2;

    const Vec2 right = {fix32::from_int(1), fix32{}};
    const Vec2 spun = rotate(rotation(turns(QUARTER_TURN)), right);
    // +Y вниз, поэтому четверть оборота уводит «вправо» в «вниз». Знак здесь — не деталь: он
    // задаёт, в какую сторону крутится тело под моментом, и перепутанный даёт мир, где колесо
    // едет назад.
    check(spun.x.raw == 0 && spun.y.raw == ONE, "a quarter turn maps (1,0) to (0,1)");

    const Vec2 identity = rotate(rotation(turns(0)), {fix32::from_int(123), fix32::from_int(-45)});
    check(identity.x.raw == fix32::from_int(123).raw && identity.y.raw == fix32::from_int(-45).raw,
          "a zero turn is the identity");

    const Vec2 v = {fix32::from_int(1000), fix32{}};
    const fix32 expected = framework::length(v);
    int32_t worst = 0, worst_trip = 0;
    for (int32_t a = 0; a < ONE; a += STEP) {
        const framework::Rot r = rotation(turns(a));
        const int32_t delta = framework::length(rotate(r, v)).raw - expected.raw;
        const int32_t mag = delta < 0 ? -delta : delta;
        if (mag > worst) worst = mag;
        // Обратный поворот возвращает вектор с точностью до двух округлений, поэтому сравнение не
        // побитовое. Оно всё равно жёсткое: сломанный знак в `unrotate` даёт отклонение порядка
        // самой длины, а не порядка допуска.
        const int32_t trip = framework::length(framework::unrotate(r, rotate(r, v)) - v).raw;
        if (trip > worst_trip) worst_trip = trip;
    }
    std::printf("  rotation: worst length drift = %d raw, round trip = %d raw\n",
                worst, worst_trip);
    check(worst <= LENGTH_TOLERANCE, "rotation preserves the length of a vector");
    check(worst_trip <= LENGTH_TOLERANCE, "unrotate(rotate(v)) returns the original vector");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework trig gate\n");

    test_exact_nodes();
    test_symmetry_and_period();
    test_unit_circle();
    test_monotonic_quarter();
    test_rotate();

    std::printf("framework trig: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
