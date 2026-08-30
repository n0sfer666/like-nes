#include <cstdio>

#include "framework_graphics_particle_scene.hpp"
#include "platform_args.hpp"

// Геймплейные частицы: гейт 4 спеки #17 — тот же ввод даёт тот же хеш состояния эмиттеров. Здесь же
// живут утверждения о ГЕОМЕТРИИ и об отрисовке, потому что голден на них не отвечает: сдвинутый на
// тик возраст даёт стабильный хеш на всех трёх машинах ровно так же, как верный.
//
// ОТКАЗЫ живут в `..._particle_refusal_test`, РАЗДЕЛЕНИЕ КЛАССОВ (гейт 3) — в
// `..._particle_split_test`: имя упавшей цели обязано называть класс поломки.
namespace {

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

using namespace framework;
using namespace framework::graphics;

constexpr uint64_t GOLDEN = 0xab81c90241aaa65aull;

constexpr uint32_t CAP = 1024;
Particle pool_a[CAP];
Particle pool_b[CAP];
Sprite sprites[CAP];
uint64_t keys[CAP];
Batch batches[CAP];

const fix32 ONE = fix32::from_int(1);

uint64_t run(Particle* pool, uint32_t cap, uint32_t ticks, uint32_t* live, uint32_t* dropped) {
    GameplayEmitter e(pool, cap, scene::table(), scene::DESCS, 0x2545f491u);
    for (uint32_t t = 0; t < ticks; ++t) {
        scene::feed(e, t);
        e.step();
    }
    if (live != nullptr) *live = e.count();
    if (dropped != nullptr) *dropped = e.dropped();
    return scene::fold(e);
}

// Гейт 4 буквально: тот же ввод — тот же хеш. Повтор берётся на ЧУЖОМ буфере, потому что общий
// прошёл бы и у реализации, которая читает мусор, оставшийся от прошлого прогона.
void test_replay_matches() {
    const uint64_t a = run(pool_a, CAP, 120, nullptr, nullptr);
    const uint64_t b = run(pool_b, CAP, 120, nullptr, nullptr);
    check(a == b, "the same input gives the same emitter state");
    check(a != scene::fold(GameplayEmitter(pool_a, CAP, scene::table(), scene::DESCS, 0x2545f491u)),
          "and the scene is not vacuously empty");
}

// Поток случайности не следует за ЁМКОСТЬЮ: сцена с большим пулом обязана дать тот же поток, что и
// с маленьким. Иначе «частиц стало больше» тихо меняло бы игру — потери сдвигали бы генератор.
void test_stream_does_not_follow_capacity() {
    uint32_t small_lost = 0;
    uint32_t big_lost = 0;
    GameplayEmitter s(pool_a, 16, scene::table(), scene::DESCS, 7u);
    GameplayEmitter b(pool_b, CAP, scene::table(), scene::DESCS, 7u);
    for (uint32_t t = 0; t < 60; ++t) {
        scene::feed(s, t);
        scene::feed(b, t);
        s.step();
        b.step();
    }
    small_lost = s.dropped();
    big_lost = b.dropped();
    std::printf("  stream: pool 16 lost %u, pool %u lost %u\n", small_lost, CAP, big_lost);
    check(s.stream() == b.stream(), "the stream is the same whatever the pool holds");
    check(small_lost > 0, "and the small pool really did overflow");
    check(big_lost == 0, "while the big one did not");
}

// Частица живёт РОВНО заказанное число тиков, и порядок переживает смерть соседа: игра-образец
// уплотняет обменом с последней, здесь — сдвигом, и разница видна только так.
void test_life_and_order() {
    EmitDesc d[2];
    d[0].life_ticks = 2;
    d[0].region = 1;
    d[1].life_ticks = 9;
    d[1].region = 2;
    GameplayEmitter e(pool_a, CAP, d, 2, 1u);
    e.burst(1, {ONE, fix32{}}, 1);
    e.burst(0, {fix32::from_int(2), fix32{}}, 1);
    e.burst(1, {fix32::from_int(3), fix32{}}, 1);
    check(e.count() == 3, "three particles are alive");
    e.step();
    check(e.count() == 3, "and none of them died after one tick");
    e.step();
    check(e.count() == 2, "the short-lived one died exactly at its life");
    check(e.at(0).pos.x == ONE && e.at(1).pos.x == fix32::from_int(3),
          "and the survivors kept their order");
}

// Геометрия шага, посчитанная руками: конус нулевой ширины смотрит вдоль +X, тяготение прибавляется
// ДО движения, трение — множителем за тик.
void test_step_geometry() {
    EmitDesc d[1];
    d[0].speed_min = fix32::from_int(4);
    d[0].speed_max = fix32::from_int(4);
    d[0].gravity = {fix32{}, ONE};
    d[0].damping = fix32::from_float(0.5);
    d[0].life_ticks = 100;
    d[0].region = 1;
    GameplayEmitter e(pool_a, CAP, d, 1, 3u);
    e.burst(0, {fix32{}, fix32{}}, 1);
    check(e.at(0).vel.x == fix32::from_int(4) && e.at(0).vel.y == fix32{},
          "a zero-width cone points along +X");
    e.step();
    check(e.at(0).vel == Vec2{fix32::from_int(2), fix32::from_float(0.5)},
          "gravity lands before damping, damping is a per-tick factor");
    check(e.at(0).pos == Vec2{fix32::from_int(2), fix32::from_float(0.5)},
          "and the position moved by the NEW velocity");
}

// Отрисовка: размер и цвет идут по возрасту, регион 0 не рисуется вовсе, один материал — один батч.
void test_draw() {
    EmitDesc d[2];
    d[0].half_start = fix32::from_int(4);
    d[0].half_end = fix32::from_int(8);
    d[0].rgba_start = 0x00000000u;
    d[0].rgba_end = 0x80402010u;
    d[0].life_ticks = 4;
    d[0].region = 3;
    d[0].material = 6;
    d[0].layer = -3;
    d[1].life_ticks = 4;
    d[1].region = 0;
    GameplayEmitter e(pool_a, CAP, d, 2, 5u);
    e.burst(0, {fix32{}, fix32{}}, 1);
    e.burst(1, {fix32{}, fix32{}}, 1);
    SpriteList list(sprites, keys, CAP);
    check(e.draw(list) == 1, "the region-0 particle draws nothing");
    check(list.drawn(0).half.x == fix32::from_int(4) && list.drawn(0).rgba == 0x00000000u,
          "a fresh particle takes the start of both curves");
    e.step();
    e.step();
    list.clear();
    e.draw(list);
    check(list.drawn(0).half.x == fix32::from_int(6), "half-way through, size is half-way too");
    check(list.drawn(0).rgba == 0x40201008u, "and so is every channel of the colour");
    check(list.drawn(0).region == 3 && list.drawn(0).material == 6 && list.drawn(0).layer == -3,
          "region, material and layer come from the desc");
    check(list.build(batches, CAP) == 1, "one material is one draw call");
}

// Непрерывный источник копит ДРОБНЫЙ остаток: 2.5 частицы за тик обязаны дать ровно 10 за четыре
// тика, а не 8 (потерянный остаток) и не 12 (округление вверх на каждом такте).
void test_continuous_rate() {
    EmitDesc d[1];
    d[0].rate_per_tick = fix32::from_float(2.5);
    d[0].life_ticks = 1000;
    d[0].region = 1;
    GameplayEmitter e(pool_a, CAP, d, 1, 9u);
    for (uint32_t t = 0; t < 4; ++t) e.emit(0, {fix32{}, fix32{}}, ONE);
    check(e.count() == 10, "2.5 per tick over four ticks is ten particles");
    for (uint32_t t = 0; t < 4; ++t) e.emit(0, {fix32{}, fix32{}}, fix32::from_float(0.5));
    check(e.count() == 15, "and a half-tick step gives half as many");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("gameplay particles\n");

    test_replay_matches();
    test_stream_does_not_follow_capacity();
    test_life_and_order();
    test_step_geometry();
    test_draw();
    test_continuous_rate();

    uint32_t live = 0;
    uint32_t lost = 0;
    const uint64_t h = run(pool_a, CAP, 300, &live, &lost);
    // Живых печатаем не для красоты: голден на пустой сцене совпал бы на трёх машинах ничуть не
    // хуже, чем на полной, и молчал бы ровно так же.
    std::printf("  particles: 300 ticks, %u alive, lost %u\n", live, lost);
    std::printf("  particle-sim hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == GOLDEN, "the emitter state matches the golden");

    std::printf("framework-graphics-particle: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
