#include "belt_ride.hpp"
#include "depth_move.hpp"
#include "framework_depth_floor_fixture.hpp"

#include <cstdint>
#include <cstdio>

#include "platform_args.hpp"

// Спека #29 В2б: ход тела belt и лента — ядро SDK, а «когда прыжок» и лицо — игра. Каждое правило
// обязано отбить свою сломанную копию: без клампа стик разгоняет тело выше скорости, с перепутанными
// скоростями `z` идёт скоростью `x`, нулевое намерение без сброса оставляет тело идти, подскок без
// проверки земли отталкивается от воздуха, касание пола без обнуления оставляет тело падающим или под
// полом, лента без проверки земли везёт прыгнувшего, а лента в обход пола проходит стену.
namespace {

using framework::depth::DepthBody;
using framework::depth::DepthFloor;
using framework::depth::DepthIntent;
using framework::depth::DepthProfile;
using framework::depth::FloorView;
using framework::depth::grounded;
using namespace framework::depth::test;

const DepthProfile PROFILE{px(3), px(2), px(1), px(8)};
const fix32 HALF = fix32::from_raw(1 << 15);
constexpr int TICKS = 32;

using Driver = void (*)(DepthBody&, const DepthIntent&, const DepthProfile&);
using Faller = void (*)(DepthBody&, const DepthProfile&);
using Rider = void (*)(DepthBody&, fix32, FloorView);

fix32 unit(fix32 v) { return v < -px(1) ? -px(1) : (px(1) < v ? px(1) : v); }

void drive_unclamped(DepthBody& d, const DepthIntent& in, const DepthProfile& p) {
    d.vx = in.mx * p.speed_x;
    d.vz = in.mz * p.speed_z;
    if (grounded(d) && in.hop) d.vy = p.jump_vy;
}

void drive_swapped(DepthBody& d, const DepthIntent& in, const DepthProfile& p) {
    d.vx = unit(in.mx) * p.speed_x;
    d.vz = unit(in.mz) * p.speed_x;
    if (grounded(d) && in.hop) d.vy = p.jump_vy;
}

void drive_keeps_speed(DepthBody& d, const DepthIntent& in, const DepthProfile& p) {
    if (in.mx == fix32{} && in.mz == fix32{} && !in.hop) return;
    framework::depth::drive(d, in, p);
}

void drive_hop_in_air(DepthBody& d, const DepthIntent& in, const DepthProfile& p) {
    d.vx = unit(in.mx) * p.speed_x;
    d.vz = unit(in.mz) * p.speed_z;
    if (in.hop) d.vy = p.jump_vy;
}

bool drive_holds(Driver drive) {
    DepthBody far{};
    drive(far, DepthIntent{px(3), -px(3), false}, PROFILE);
    DepthBody half{};
    drive(half, DepthIntent{HALF, -HALF, false}, PROFILE);
    DepthBody ground{};
    drive(ground, DepthIntent{fix32{}, fix32{}, true}, PROFILE);
    DepthBody air{};
    air.y = px(5);
    air.vy = -px(1);
    drive(air, DepthIntent{fix32{}, fix32{}, true}, PROFILE);
    DepthBody walking{};
    walking.vx = px(3);
    walking.vz = px(2);
    drive(walking, DepthIntent{}, PROFILE);
    return far.vx == px(3) && far.vz == -px(2) && half.vx == PROFILE.speed_x * HALF &&
           half.vz == -(PROFILE.speed_z * HALF) && ground.vy == PROFILE.jump_vy && air.vy == -px(1) &&
           walking.vx == fix32{} && walking.vz == fix32{} && walking.vy == fix32{};
}

void fall_through(DepthBody& d, const DepthProfile& p) {
    if (!(fix32{} < d.y) && !(fix32{} < d.vy)) return;
    d.vy = d.vy - p.gravity;
    d.y = d.y + d.vy;
}

void fall_keeps_speed(DepthBody& d, const DepthProfile& p) {
    fall_through(d, p);
    if (d.y < fix32{}) d.y = fix32{};
}

DepthBody fallen(Faller fall, fix32 y, fix32 vy) {
    DepthBody b{};
    b.y = y;
    b.vy = vy;
    for (int t = 0; t < TICKS; ++t) fall(b, PROFILE);
    return b;
}

// С 11 px шаг тяжести перескакивает пол (10, 8, 5, 1, −4): касание обязано встать ровно на ноль и
// погасить `vy`. Подскок возвращается на пол тем же правилом.
bool fall_holds(Faller fall) {
    const DepthBody dropped = fallen(fall, px(11), fix32{});
    const DepthBody hopped = fallen(fall, fix32{}, PROFILE.jump_vy);
    const DepthBody still = fallen(fall, fix32{}, fix32{});
    return grounded(dropped) && grounded(hopped) && grounded(still);
}

void ride_in_air(DepthBody& d, fix32 belt, FloorView f) {
    if (belt == fix32{}) return;
    d.x = framework::depth::move_x(f, d.x, d.z, d.x + belt);
}

void ride_through(DepthBody& d, fix32 belt, FloorView) {
    if (belt == fix32{} || !grounded(d)) return;
    d.x = d.x + belt;
}

DepthBody ridden(Rider ride, fix32 x, fix32 z, fix32 y) {
    const DepthFloor<> f = walled();
    DepthBody b{x, z, y, px(1), fix32{}, fix32{}};
    ride(b, px(3), f.view());
    return b;
}

bool belt_holds(Rider ride) {
    const DepthBody carried = ridden(ride, px(20), px(20), fix32{});
    return carried.x == px(23) && carried.vx == px(1) && ridden(ride, px(20), px(20), px(4)).x == px(20) &&
           ridden(ride, px(39), px(20), fix32{}).x == px(40) && ridden(ride, px(99), px(45), fix32{}).x == px(100);
}

void test_drive() {
    check(drive_holds(framework::depth::drive), "the stick, the speeds or the hop are off");
    check(!drive_holds(drive_unclamped), "control: a stick without the clamp holds");
    check(!drive_holds(drive_swapped), "control: z at the x speed holds");
    check(!drive_holds(drive_hop_in_air), "control: a hop from the air holds");
    check(!drive_holds(drive_keeps_speed), "control: a zero intent that keeps the speed holds");
}

void test_fall() {
    check(fall_holds(framework::depth::fall), "the body does not land on the floor with vy zero");
    check(!fall_holds(fall_through), "control: a fall through the floor holds");
    check(!fall_holds(fall_keeps_speed), "control: a landing that keeps the speed holds");
}

void test_belt() {
    const Rider real = framework::depth::ride_belt;
    check(belt_holds(real), "the belt carries a body in the air or through a wall");
    check(!belt_holds(ride_in_air), "control: a belt that carries a body in the air holds");
    check(!belt_holds(ride_through), "control: a belt past the floor holds");
    const DepthFloor<> f = walled();
    DepthBody b{px(39), px(20), fix32{}, fix32{}, fix32{}, fix32{}};
    ride_belt(b, px(3), f);
    check(b.x == px(40), "the floor overload stops on the wall face");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework depth move: the stick, the hop, the fall and the belt\n");
    test_drive();
    test_fall();
    test_belt();
    std::printf("framework-depth-move: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
