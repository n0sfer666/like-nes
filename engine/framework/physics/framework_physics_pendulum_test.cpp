#include "pendulum.hpp"

#include <cstdint>
#include <cstdio>

#include "fixtrig.hpp"
#include "framework_physics_pendulum_energy.hpp"
#include "platform_args.hpp"

// Спека #29 В2б: маятник — ядро физики SDK; седок, раскачка от стика и остывание — драка. Каждое
// утверждение обязано отбить свою сломанную копию: без затухания теневая энергия сохраняется и пик
// последнего окна не опускается ниже начала; без обнуления, без клампа и с остановкой ровно на
// пределе ломается размах; с перепутанными синусом и косинусом — конец, со знаком `+sin` — касательная.
namespace {

using framework::Vec2;
using framework::physics::Pendulum;
using framework::physics::pendulum_cap;
using framework::physics::pendulum_end;
using framework::physics::pendulum_tangent;
using framework::physics::pendulum_turns;
using framework::physics::swing_pendulum;
using framework::physics::test::EnergyGate;
using framework::physics::test::pendulum_energy;

constexpr fix32 G = fix32::from_raw(16384);
constexpr int32_t DAMPING = 2048;
constexpr uint32_t TICKS = 10000;
constexpr int32_t LENGTHS[] = {16, 96, 256};
constexpr int32_t DEGREES[] = {5, 45, 89};

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

enum class Cap { Honest, NoStop, NoClamp, StopOnCap };

using Swinger = void (*)(Pendulum&, fix32);

template <bool Damped, Cap Rule>
void swing(Pendulum& p, fix32 length) {
    if (Damped && Rule == Cap::Honest) return swing_pendulum(p, length, G, fix32{}, DAMPING);
    p.speed = p.speed - G * framework::sin_turns(pendulum_turns(p.arc, length));
    p.arc = p.arc + p.speed;
    if (Damped) p.speed = p.speed - fix32::from_raw(p.speed.raw / DAMPING);
    const fix32 cap = pendulum_cap(length);
    const fix32 reach = framework::abs_fix(p.arc);
    if (Rule == Cap::NoClamp || (Rule == Cap::StopOnCap ? reach < cap : !(cap < reach))) return;
    p.arc = framework::clamp_fix(p.arc, -cap, cap);
    if (Rule != Cap::NoStop) p.speed = fix32{};
}

bool settles(Swinger step, int32_t length, int32_t degrees) {
    const fix32 l = fix32::from_int(length);
    Pendulum p{pendulum_cap(l) * fix32::from_int(degrees) / fix32::from_int(90), fix32{}};
    EnergyGate gate(pendulum_energy(p, l, G));
    for (uint32_t t = 0; t < TICKS; ++t) {
        step(p, l);
        gate.add(pendulum_energy(p, l, G));
    }
    return gate.settles();
}

void test_energy_does_not_grow_without_push() {
    for (const int32_t l : LENGTHS)
        for (const int32_t d : DEGREES) {
            const bool ok = settles(swing<true, Cap::Honest>, l, d);
            const bool broken = settles(swing<false, Cap::Honest>, l, d);
            if (!ok || broken) std::printf("    length %d, %d degrees: honest %d, undamped %d\n", l, d, ok, broken);
            check(ok, "the pendulum gains energy without a push");
            check(!broken, "control: the pendulum without damping passes the energy gate");
        }
}

// Дуга, с которой шаг приходит ровно на предел: скорость после тяжести зависит от самой дуги, и
// подбор сходится за пару итераций — у предела синус плоский.
Pendulum onto_cap(fix32 length, fix32 sign) {
    const fix32 cap = pendulum_cap(length) * sign;
    const fix32 speed = fix32::from_int(2) * sign;
    Pendulum p{cap - speed, speed};
    for (int k = 0; k < 8; ++k) p.arc = cap - (speed - G * framework::sin_turns(pendulum_turns(p.arc, length)));
    return p;
}

// Шаг за предел встаёт на нём со скоростью ноль, шаг ровно на предел идёт дальше со скоростью, шаг
// внутри предела не останавливается.
bool caps_hold(Swinger step) {
    bool ok = true;
    for (const int32_t l : LENGTHS) {
        const fix32 length = fix32::from_int(l);
        const fix32 cap = pendulum_cap(length);
        for (const int sign : {1, -1}) {
            const fix32 s = fix32::from_int(sign);
            Pendulum past{(cap - fix32::from_raw(1)) * s, fix32::from_int(4) * s};
            step(past, length);
            ok = ok && past.arc.raw == (cap * s).raw && past.speed.raw == 0;
            Pendulum onto = onto_cap(length, s);
            step(onto, length);
            ok = ok && onto.arc.raw == (cap * s).raw && onto.speed.raw != 0;
        }
        Pendulum inside{cap - fix32::from_int(1), fix32{}};
        step(inside, length);
        ok = ok && inside.speed.raw != 0;
    }
    return ok;
}

void test_the_swing_stops_at_the_cap() {
    check(caps_hold(swing<true, Cap::Honest>), "the swing does not stop at the cap, or stops short of it");
    check(!caps_hold(swing<true, Cap::NoStop>), "control: a clamp that keeps the speed holds the cap");
    check(!caps_hold(swing<true, Cap::NoClamp>), "control: a swing without the clamp holds the cap");
    check(!caps_hold(swing<true, Cap::StopOnCap>), "control: a stop on reaching the cap exactly holds the cap");
}

using EndFn = Vec2 (*)(Vec2, fix32, fix32);
using TangentFn = Vec2 (*)(fix32, fix32);

Vec2 end_swapped(Vec2 pivot, fix32 length, fix32 arc) {
    const fix32 t = pendulum_turns(arc, length);
    return {pivot.x + length * framework::cos_turns(t), pivot.y + length * framework::sin_turns(t)};
}

Vec2 tangent_flipped(fix32 arc, fix32 length) {
    const fix32 t = pendulum_turns(arc, length);
    return {framework::cos_turns(t), framework::sin_turns(t)};
}

bool same(Vec2 a, fix32 x, fix32 y) { return a.x.raw == x.raw && a.y.raw == y.raw; }

// В отвесе конец под опорой, касательная `+X`; на пределе конец на уровне опоры, касательная
// вертикальна. Длины кратны 4: предел считается без округления, и таблица на четверти точна.
bool geometry_holds(EndFn end, TangentFn tangent) {
    const Vec2 pivot{fix32::from_int(600), fix32::from_int(60)};
    const fix32 one = fix32::from_int(1);
    bool ok = true;
    for (const int32_t l : LENGTHS) {
        const fix32 length = fix32::from_int(l);
        const fix32 cap = pendulum_cap(length);
        ok = ok && same(end(pivot, length, fix32{}), pivot.x, pivot.y + length);
        ok = ok && same(tangent(fix32{}, length), one, fix32{});
        ok = ok && same(end(pivot, length, cap), pivot.x + length, pivot.y);
        ok = ok && same(end(pivot, length, -cap), pivot.x - length, pivot.y);
        ok = ok && same(tangent(cap, length), fix32{}, -one);
        ok = ok && same(tangent(-cap, length), fix32{}, one);
    }
    return ok;
}

void test_end_and_tangent() {
    check(geometry_holds(pendulum_end, pendulum_tangent), "the end or the tangent is off in the plumb or at the cap");
    check(!geometry_holds(end_swapped, pendulum_tangent), "control: an end with sine and cosine swapped holds");
    check(!geometry_holds(pendulum_end, tangent_flipped), "control: a tangent with +sin holds");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics pendulum: the swing loses energy without a push and stops at the cap\n");
    test_energy_does_not_grow_without_push();
    test_the_swing_stops_at_the_cap();
    test_end_and_tangent();
    std::printf("framework-physics-pendulum: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
