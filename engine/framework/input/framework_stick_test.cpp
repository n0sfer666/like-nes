#include "stick.hpp"

#include <cstdio>

#include "platform_args.hpp"

// Гейт 3 спеки #14: границы мёртвой зоны, диагонали, кривые и дребезг у порога — таблицей
// эталонных значений. Эталоны посчитаны от определения (а не снятием текущего вывода), поэтому
// сравнение идёт с допуском в один-два младших разряда Q16.16 там, где участвует корень.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using framework::input::StickShape;
using framework::input::Vec2;

bool near_raw(fix32 got, fix32 want, int32_t tol) {
    const int32_t d = got.raw - want.raw;
    return (d < 0 ? -d : d) <= tol;
}

void check_near(fix32 got, double want, int32_t tol, const char* what) {
    const fix32 w = fix32::from_float(want);
    if (!near_raw(got, w, tol)) {
        std::printf("  FAIL: %s (got %.6f, want %.6f)\n", what, got.to_double(), want);
        ++fails;
    }
}

fix32 f(double v) { return fix32::from_float(v); }

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    using namespace framework::input;

    // Корень: якоря, где точное значение представимо, — иначе допуск скрыл бы систематический сдвиг.
    check(sqrt_fix(f(1.0)) == f(1.0), "sqrt(1) == 1");
    check(sqrt_fix(f(0.25)) == f(0.5), "sqrt(0.25) == 0.5");
    check(sqrt_fix(f(4.0)) == f(2.0), "sqrt(4) == 2");
    check(sqrt_fix(fix32{}) == fix32{}, "sqrt(0) == 0");
    check(sqrt_fix(f(-1.0)) == fix32{}, "sqrt of a negative value is zero, not a wrapped root");
    check_near(sqrt_fix(f(2.0)), 1.4142135, 2, "sqrt(2)");

    StickShape s;
    s.deadzone = f(0.25);
    s.outer = fix32::from_int(1);
    s.curve_exp = 1;

    // Одна ось: точно на границе зоны — ноль, сразу за ней — почти ноль, на краю — единица.
    check(axial(f(0.25), s) == fix32{}, "exactly at the deadzone edge the axis is silent");
    check(axial(f(-0.25), s) == fix32{}, "the deadzone is symmetric");
    check_near(axial(f(0.26), s), 0.0133333, 2, "just past the edge the axis barely moves");
    check_near(axial(f(0.625), s), 0.5, 2, "the remainder is normalised, not offset");
    check(axial(f(1.0), s) == fix32::from_int(1), "full deflection reaches exactly one");
    check(axial(f(2.0), s) == fix32::from_int(1), "an over-range axis is clamped, not amplified");
    check_near(axial(f(-0.625), s), -0.5, 2, "the sign survives normalisation");

    // Ступеньки на медленном движении (риск спеки): монотонность и отсутствие плато шире
    // одного шага. Плато в два шага подряд означало бы, что разрядности не хватает.
    int plateau = 0, worst = 0;
    fix32 prev = axial(f(0.25), s);
    for (int i = 1; i <= 750; ++i) {
        const fix32 v = f(0.25 + 0.001 * i);
        const fix32 cur = axial(v, s);
        check(!(cur < prev), "a slowly growing axis never goes backwards");
        if (cur == prev) { if (++plateau > worst) worst = plateau; } else plateau = 0;
        prev = cur;
    }
    std::printf("  longest plateau on a 0.001 sweep: %d step(s)\n", worst);
    check(worst <= 1, "a 0.001 input step never stalls the output for two steps in a row");

    // Кривые: t^exp на нормализованном остатке, а не на сыром вводе.
    StickShape sq = s;
    sq.curve_exp = 2;
    check_near(axial(f(0.625), sq), 0.25, 2, "a squared curve halves the mid-range");
    check(axial(f(1.0), sq) == fix32::from_int(1), "a curve keeps full deflection full");
    StickShape cube = s;
    cube.curve_exp = 3;
    check_near(axial(f(0.625), cube), 0.125, 3, "a cubic curve is steeper still");

    // Радиальная зона: диагональ клавиатуры имеет модуль √2 и обязана давать единичную длину,
    // иначе по диагонали игрок ходит в 1.41 раза быстрее — классический баг платформеров.
    StickShape r;
    r.deadzone = fix32{};
    const Vec2 diag = radial({fix32::from_int(1), fix32::from_int(1)}, r);
    check_near(diag.x, 0.7071067, 3, "a keyboard diagonal is normalised to the unit circle (x)");
    check_near(diag.y, 0.7071067, 3, "a keyboard diagonal is normalised to the unit circle (y)");
    check(near_raw(diag.x, diag.y, 1), "the diagonal stays symmetric");

    // Круглая зона: точка на диагонали внутри круга радиуса 0.25 молчит, хотя КАЖДАЯ её ось
    // больше осевой зоны 0.2 — ровно то, чем радиальная зона отличается от осевой.
    StickShape rr;
    rr.deadzone = f(0.25);
    const Vec2 inside = radial({f(0.17), f(0.17)}, rr);
    check(inside.x == fix32{} && inside.y == fix32{}, "a diagonal inside the circle is silent");
    const Vec2 outside = radial({f(0.2), f(0.2)}, rr);
    check(!(outside.x == fix32{}), "a diagonal outside the circle is not");
    const Vec2 pure = radial({f(0.5), fix32{}}, rr);
    check_near(pure.x, 0.3333333, 3, "on a single axis the circle behaves like the linear zone");
    check(pure.y == fix32{}, "an untouched axis stays exactly zero");

    // Стик, выведенный в угол, даёт модуль больше единицы — длина обязана стать ровно единицей.
    const Vec2 corner = radial({fix32::from_int(1), fix32::from_int(1)}, rr);
    const fix32 corner_len = sqrt_fix(corner.x * corner.x + corner.y * corner.y);
    check(near_raw(corner_len, fix32::from_int(1), 3), "the corner of the stick box is clamped to 1");

    // Порог триггера: ниже — ровно ноль (иначе «нажат» рапортуется от шума покоящегося триггера).
    check(trigger(f(0.1), f(0.12)) == fix32{}, "below the threshold the trigger reads zero");
    check(trigger(f(0.12), f(0.12)) == fix32{}, "exactly at the threshold it is still zero");
    check_near(trigger(f(0.56), f(0.12)), 0.5, 2, "past the threshold the range is stretched to full");
    check(trigger(fix32::from_int(1), f(0.12)) == fix32::from_int(1), "a pulled trigger reads one");
    check(trigger(f(-0.5), f(0.12)) == fix32{}, "a negative reading is not a pulled trigger");

    // Кривые формы, пришедшие из ассета, могут быть любыми — функция обязана остаться тотальной.
    StickShape bad;
    bad.deadzone = fix32::from_int(2);
    bad.outer = fix32{};
    check(axial(f(0.5), bad).raw >= 0, "a nonsense shape still yields a defined value");
    check(!(fix32::from_int(1) < axial(fix32::from_int(1), bad)), "and never exceeds full deflection");

    const bool pass = (fails == 0);
    std::printf("framework-stick: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
