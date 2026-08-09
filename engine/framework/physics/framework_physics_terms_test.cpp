#include <cstdio>

#include "axis_terms.hpp"
#include "impulse.hpp"
#include "platform_args.hpp"
#include "shape.hpp"

// Коэффициенты разрешения контакта — НАПРЯМУЮ, без единого шага мира.
//
// Оба дефекта вертикали 2 жили здесь, а наблюдались за двести сорок шагов сцены: и «доля равна
// нулю», и «доля вдвое меньше нужной» дают там один и тот же исход — тело утонуло, — и прогон их не
// различает. Прямое утверждение различает: приложенное `apply_axis` изменение относительной скорости
// вдоль оси обязано равняться `lambda`. Это тождество и есть определение `k` — оно связывает
// эффективную обратную массу с долями тел, и любое их расхождение видно на ОДНОМ контакте.
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

// Допуск относительный, потому что проверяемая величина — отношение, а не абсолют: доли считаются
// делением, каждое из четырёх слагаемых теряет младший разряд, и на малых `lambda` эта потеря видна
// в процентах. Полпроцента — с запасом ниже того, что ловится: дефекты этого класса дают половину
// нужной величины или ноль, а не проценты.
constexpr fix32 TOLERANCE = fix32::from_float(0.005);

// Отдельно от относительного — абсолютный пол: при `lambda` в единицы юнитов относительная ошибка
// одного разряда сама доходит до процента, и требовать там доли процента значило бы измерять
// округление, а не физику.
constexpr fix32 FLOOR = fix32::from_float(0.02);

struct Setup {
    const char* what;
    BodyDesc a;
    BodyDesc b;
    Vec2 anchor;   // точка контакта в мировых координатах
    Vec2 axis;     // единичная
    int64_t lambda_raw;
};

// Тождество k: сколько накопленного вложено, столько относительной скорости и получено.
void check_identity(const Setup& s) {
    Body a = make_body(s.a);
    Body b = make_body(s.b);
    ManifoldPoint p;
    p.anchor_a = s.anchor - a.position;
    p.anchor_b = s.anchor - b.position;

    const AxisTerms t = axis_terms(a, b, p.anchor_a, p.anchor_b, s.axis);
    const fix32 before = dot(relative_velocity(a, b, p), s.axis);
    apply_axis(a, b, t, s.axis, s.lambda_raw);
    const fix32 after = dot(relative_velocity(a, b, p), s.axis);

    const fix32 want = fix32::from_raw(fix32::sat(s.lambda_raw));
    const fix32 got = after - before;
    const fix32 slack = max_fix(FLOOR, abs_fix(want) * TOLERANCE);
    if (!(abs_fix(got - want) < slack)) {
        std::printf("  %s: want %d/1000, got %d/1000 (k=%d/1000, ang_a=%d/100000)\n", s.what,
                    (want * fix32::from_int(1000)).to_int(), (got * fix32::from_int(1000)).to_int(),
                    (t.k * fix32::from_int(1000)).to_int(),
                    (t.ang_a * fix32::from_int(100000)).to_int());
    }
    check(abs_fix(got - want) < slack, s.what);
}

BodyDesc dynamic_box(fix32 half, fix32 mass, Vec2 at) {
    BodyDesc d;
    d.shape = box(half, half);
    d.mass = mass;
    d.position = at;
    return d;
}

BodyDesc static_floor() {
    BodyDesc d;
    d.key = 99;
    d.type = BodyType::Static;
    d.shape = box(fix32::from_int(400), fix32::from_int(8));
    d.position = {fix32{}, fix32::from_int(40)};
    return d;
}

void test_axis_identity() {
    const Vec2 down = {fix32{}, fix32::from_int(1)};
    const Vec2 side = {fix32::from_int(1), fix32{}};

    // Плечо нулевое: вращение из тождества выпадает целиком, и остаётся чистая проверка линейных
    // долей. Если она разойдётся, все остальные случаи ниже читать бесполезно.
    Setup centered{"a head-on contact spends lambda on velocity alone",
                   dynamic_box(fix32::from_int(16), fix32::from_int(1), {fix32{}, fix32::from_int(16)}),
                   static_floor(),
                   {fix32{}, fix32::from_int(32)},
                   down,
                   fix32::from_int(4).raw};
    check_identity(centered);

    // То же тело, но контакт у КРАЯ формы: плечо максимально, и угловая доля забирает заметную часть
    // импульса. Именно этот случай был сломан — вклад вращения стоял в `k`, а применялся нулём.
    Setup offset = centered;
    offset.what = "an off-centre contact still spends exactly lambda";
    offset.anchor = {fix32::from_int(16), fix32::from_int(32)};
    check_identity(offset);

    // Тождество выше проверяет СОГЛАСОВАННОСТЬ `k` и долей, и этого мало: посчитай вклад вращения
    // вдвое меньше — и `k`, и доли поедут вместе, тождество останется верным, а тело будет отвечать
    // на удар вдвое охотнее, чем положено его форме. Величину поэтому надо пинить абсолютом, и он
    // выводится руками: у коробки 32x32 момент на единицу массы равен (32^2 + 32^2)/12 = 170.67,
    // плечо от центра до угла по оси Y равно 16, значит вклад = 1 * 16^2 / 170.67 = 1.5, а
    // k = 1 (обратная масса) + 1.5 + 0 (пол — статика) = 2.5.
    Body ka = make_body(offset.a);
    Body kb = make_body(offset.b);
    const AxisTerms kt =
        axis_terms(ka, kb, offset.anchor - ka.position, offset.anchor - kb.position, down);
    check(abs_fix(kt.k - fix32::from_float(2.5)) < fix32::from_float(0.01),
          "the effective inverse mass matches the hand-derived 2.5");

    // Та же геометрия при массе 384 — ровно там, где прежняя схема округляла обратный момент в ноль.
    // Оба слагаемых делятся на массу, поэтому k = 2.5/384 = 0.006510. Пин здесь именно абсолютный:
    // «вращается» подписалось бы и под вдесятеро меньшим вкладом.
    ka.inv_mass = fix32::from_int(1) / fix32::from_int(384);
    const AxisTerms ht =
        axis_terms(ka, kb, offset.anchor - ka.position, offset.anchor - kb.position, down);
    check(abs_fix(ht.k - fix32::from_float(0.006510)) < fix32::from_float(0.0005),
          "and it scales with mass instead of rounding to zero");

    // Верхний край диапазона масс на КОРОТКОМ плече — раскладка, где вклад вращения целиком лежит
    // ниже младшего разряда `k`: у коробки 32x32 массой 4096 на плече 3.2 он равен 62912 в Q16.32,
    // то есть ровно ноль после перевода в шкалу `k`. Посчитай долю из полного значения — и в `k` не
    // попадёт ни разряда, а приложится 1.06 * lambda. Шесть процентов лишнего отпора на каждом
    // контакте: наблюдаемо это тяжёлый ящик, который пружинит тем заметнее, чем он тяжелее, — и
    // тождество единственное, чем это отличается от «так и задумано».
    Setup fine = centered;
    fine.what = "a contribution below the resolution of k is dropped from the share as well";
    fine.a.mass = MAX_MASS;
    fine.anchor = {fix32::from_float(3.2), fix32::from_int(32)};
    check_identity(fine);

    // Оба берега массы 384, на которой обратный момент инерции округлялся в ноль. Проверяется не
    // «вращается», а то же тождество: доля и `k` обязаны сходиться на любой законной массе.
    const fix32 masses[] = {fix32::from_int(1),    fix32::from_int(383), fix32::from_int(384),
                            fix32::from_int(1024), MAX_MASS,             MIN_MASS};
    for (fix32 m : masses) {
        Setup heavy = offset;
        heavy.what = "the identity holds across the whole declared mass range";
        heavy.a.mass = m;
        check_identity(heavy);
    }

    // ДВА подвижных тела — не «ещё один случай», а вторая половина проверяемого. У статики нулевая
    // обратная масса, значит нулевой вклад и нулевая доля: сломай долю тела `b` любым множителем, и
    // все раскладки выше останутся зелёными, потому что там `b` — пол. Тождество здесь тратит
    // `lambda` на ЧЕТЫРЕ величины сразу, и любая из них, посчитанная не тем выражением, видна.
    Setup pair{"a dynamic pair splits lambda across all four terms",
               dynamic_box(fix32::from_int(16), fix32::from_int(1), {fix32{}, fix32::from_int(16)}),
               dynamic_box(fix32::from_int(16), fix32::from_int(3), {fix32{}, fix32::from_int(48)}),
               {fix32::from_int(16), fix32::from_int(32)},
               down,
               fix32::from_int(4).raw};
    check_identity(pair);
    const Body pa = make_body(pair.a);
    const Body pb = make_body(pair.b);
    const AxisTerms pt = axis_terms(pa, pb, pair.anchor - pa.position, pair.anchor - pb.position, down);
    check(pt.ang_a.raw != 0 && pt.ang_b.raw != 0, "control: both angular shares are actually live");

    // Касательная ось на том же контакте: у неё своё `k` (плечо входит в нормаль и в касательную
    // по-разному), и предел конуса переводится между ними отношением. Разойдись тождество здесь, и
    // трение либо выпускалось бы за конус, либо душилось.
    Setup tangential = offset;
    tangential.what = "the tangent axis obeys the same identity";
    tangential.axis = side;
    check_identity(tangential);

    // Зеркало по X: плечо меняет знак, `k` обязан остаться прежним (в него плечо входит квадратом),
    // а модуль угловой доли — совпасть до разряда. Асимметрия здесь означала бы, что зеркальная
    // сцена не является зеркалом исходной, — детерминизму не вредит, физике вредит.
    Body la = make_body(offset.a);
    Body lb = make_body(offset.b);
    const Vec2 left = {fix32::from_int(-16), fix32::from_int(32)};
    const Vec2 right = {fix32::from_int(16), fix32::from_int(32)};
    const AxisTerms tl = axis_terms(la, lb, left - la.position, left - lb.position, down);
    const AxisTerms tr = axis_terms(la, lb, right - la.position, right - lb.position, down);
    // Контроль первым: на нулевой доле и нулевом `k` оба утверждения ниже зелены сами по себе, и
    // зеркало подписалось бы под выключенным вращением.
    check(tl.ang_a.raw != 0 && tl.k.raw > 0, "control: the mirrored share is live at all");
    check(tl.k == tr.k, "a mirrored arm gives the same effective mass");
    check(tl.ang_a.raw == -tr.ang_a.raw, "a mirrored arm gives the exactly opposite angular share");

    // Насыщающий угол: маленькая капсула на длинном плече — та раскладка, где вклад в `k` упирается
    // в потолок fix32 раньше, чем доля. Считай их двумя независимыми выражениями, и в окне между
    // двумя порогами решатель снова применял бы не то, что заложил.
    BodyDesc needle;
    needle.shape = capsule({fix32::from_float(-0.5), fix32{}}, {fix32::from_float(0.5), fix32{}},
                           fix32::from_float(0.25));
    needle.mass = MIN_MASS;
    needle.position = {fix32{}, fix32::from_int(20)};
    Setup saturating{"the identity survives the saturating corner", needle, static_floor(),
                     {fix32::from_int(200), fix32::from_int(20)}, down, fix32::from_int(1).raw};
    check_identity(saturating);
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics terms gate\n");
    test_axis_identity();
    std::printf("framework-physics-terms: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
