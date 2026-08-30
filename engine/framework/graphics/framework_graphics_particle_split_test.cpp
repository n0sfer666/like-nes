#include <cstdio>

#include "framework_graphics_particle_scene.hpp"
#include "platform_args.hpp"

// Гейт 3 спеки #17: прогон с декоративными эффектами и без даёт БИТ В БИТ один sim-хеш. Решение
// владельца 4 требует, чтобы класс эмиттера был частью ТИПА, — поэтому половина этого файла
// проверяется компилятором, а не прогоном: у геймплейного класса нет доли тика, у декоративного нет
// тика, и перепутать их нельзя.
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

// Разделение классов утверждается ТИПОМ. Отрицательные половины несущие: без них утверждение
// проходило бы и на одном классе с флагом, то есть ровно на том, что решение 4 запрещает.
template <class T>
concept HasStep = requires(T& t) { t.step(); };
template <class T>
concept HasAdvance = requires(T& t) { t.advance(fix32::from_int(1)); };

static_assert(HasStep<GameplayEmitter>, "the gameplay class steps by a tick");
static_assert(!HasStep<DecorEmitter>, "the decorative class has no tick to step");
static_assert(!HasAdvance<GameplayEmitter>, "the gameplay class has no fraction of a tick");
static_assert(HasAdvance<DecorEmitter>, "the decorative class lives in the frame");

constexpr uint32_t CAP = 2048;
Particle sim_pool[CAP];
Particle fx_pool[CAP];

const fix32 ONE = fix32::from_int(1);

// Декоративная подача НАРОЧНО тяжелее геймплейной: гейт ловит протечку, а протечка тем заметнее, чем
// больше чужой работы сделано между двумя шагами тика.
void decorate(DecorEmitter& fx, uint32_t frame, fix32 dt) {
    const Vec2 at{fix32::from_int(static_cast<int32_t>(frame % 17)), fix32::from_int(-3)};
    fx.burst(0, at, 12);
    fx.emit(1, at, dt);
    fx.advance(dt);
}

// Сцена симуляции. `game` — целое состояние игры, ЧИТАЮЩЕЕ поток геймплейного эмиттера: без него
// хеш утверждал бы только про частицы, а протечка случайности в игру осталась бы невидимой.
uint64_t sim_run(uint32_t ticks, uint32_t frames_per_tick, uint32_t* fx_count) {
    GameplayEmitter sim(sim_pool, CAP, scene::table(), scene::DESCS, 0x2545f491u);
    DecorEmitter fx(fx_pool, CAP, scene::table(), scene::DESCS, 0x9e3779b9u);
    uint64_t game = 0;
    uint32_t frame = 0;
    const fix32 dt = frames_per_tick == 0 ? fix32{} : ONE / fix32::from_int(static_cast<int32_t>(frames_per_tick));
    for (uint32_t t = 0; t < ticks; ++t) {
        for (uint32_t f = 0; f < frames_per_tick; ++f) decorate(fx, frame++, dt);
        scene::feed(sim, t);
        sim.step();
        game = game * 0x100000001b3ull ^ sim.stream();
    }
    if (fx_count != nullptr) *fx_count = fx.count();
    uint64_t h = scene::fold(sim);
    scene::mix(h, static_cast<int64_t>(game));
    return h;
}

// Три расписания кадров на один и тот же тик. Равенство ТРЁХ, а не двух: «с эффектами и без»
// поймает общий генератор, но пропустит зависимость симуляции от ЧАСТОТЫ кадров, а это ровно тот
// дефект, ради которого тик и отделён от кадра.
void test_decoration_does_not_reach_the_sim() {
    uint32_t none = 0;
    uint32_t one = 0;
    uint32_t many = 0;
    const uint64_t a = sim_run(150, 0, &none);
    const uint64_t b = sim_run(150, 1, &one);
    const uint64_t c = sim_run(150, 5, &many);
    std::printf("  decor: 0 fps = %u alive, 1 fps = %u alive, 5 fps = %u alive\n", none, one, many);
    check(a == b && b == c, "decoration never reaches the sim hash");
    check(none == 0, "the undecorated run really drew nothing");
    check(one > 0 && many > one, "and the decorated ones really did the work");
}

// Контроль на саму сцену: хеш обязан ЗАВИСЕТЬ хоть от чего-нибудь. Равенство трёх прогонов зелено
// вакуумно, если сцена возвращает одно и то же число при любом вводе.
void test_the_sim_hash_is_not_constant() {
    check(sim_run(150, 1, nullptr) != sim_run(151, 1, nullptr), "a different input moves the hash");
}

// Декоративный класс ОБЯЗАН слышать частоту кадров: его собственное состояние при 1 и 5 кадрах на
// тик разное, и без этого утверждения гейт выше проходил бы у эмиттера, который не делает ничего.
void test_the_decor_itself_follows_the_frame() {
    DecorEmitter slow(fx_pool, CAP, scene::table(), scene::DESCS, 1u);
    for (uint32_t f = 0; f < 20; ++f) decorate(slow, f, ONE);
    const uint64_t a = scene::fold(slow);
    DecorEmitter fast(fx_pool, CAP, scene::table(), scene::DESCS, 1u);
    for (uint32_t f = 0; f < 20; ++f) decorate(fast, f, fix32::from_float(0.25));
    check(a != scene::fold(fast), "the decorative class does hear the frame rate");
}

// Интегратор у классов ОДИН, и разъезжаются они только часами. Утверждение двойное: на целом тике
// декоративный обязан совпасть с геймплейным БИТ В БИТ, а на доле тика трение обязано быть линейным
// по шагу — множитель «за тик», применённый к кадру, гасил бы скорость тем быстрее, чем выше частота
// кадров, и это как раз тот дефект, которого не видно ни в sim-хеше, ни на глаз.
void test_the_two_classes_share_one_integrator() {
    GameplayEmitter sim(sim_pool, CAP, scene::table(), scene::DESCS, 4u);
    DecorEmitter fx(fx_pool, CAP, scene::table(), scene::DESCS, 4u);
    for (uint32_t t = 0; t < 30; ++t) {
        scene::feed(sim, t);
        scene::feed(fx, t);
        sim.step();
        fx.advance(ONE);
    }
    check(scene::fold(sim) == scene::fold(fx), "a whole tick is a whole tick in either class");

    EmitDesc d[1];
    d[0].speed_min = fix32::from_int(8);
    d[0].speed_max = fix32::from_int(8);
    d[0].damping = fix32::from_float(0.5);
    d[0].life_ticks = 50;
    d[0].region = 1;
    DecorEmitter half(fx_pool, CAP, d, 1, 4u);
    half.burst(0, {}, 1);
    half.advance(fix32::from_float(0.5));
    check(half.at(0).vel.x == fix32::from_int(6), "half a tick of damping is half the loss");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("particle classes are split by type\n");

    test_decoration_does_not_reach_the_sim();
    test_the_sim_hash_is_not_constant();
    test_the_decor_itself_follows_the_frame();
    test_the_two_classes_share_one_integrator();

    std::printf("framework-graphics-particle-split: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
