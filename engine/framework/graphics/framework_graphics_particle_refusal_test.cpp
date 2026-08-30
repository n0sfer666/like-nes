#include <cstdio>

#include "framework_graphics_particle_scene.hpp"
#include "platform_args.hpp"

// Отказы частиц (спека #17, вертикаль 2, шаг C). Не родившаяся частица снаружи выглядит РОВНО как
// частица, которой не заказывали, — поэтому каждый отказ обязан быть посчитан, и каждый счёт
// проверен числом, а не знаком «больше нуля».
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

constexpr uint32_t CAP = 64;
Particle pool[CAP];
Sprite sprites[8];
uint64_t keys[8];

const fix32 ONE = fix32::from_int(1);

EmitDesc* one_desc() {
    static EmitDesc d[2];
    d[0] = EmitDesc{};
    d[0].life_ticks = 30;
    d[0].region = 1;
    d[1] = EmitDesc{};
    d[1].life_ticks = 0;
    d[1].region = 1;
    return d;
}

// Буфера нет — ёмкость ноль, а не заявленное число: иначе первая же подача писала бы по нулевому
// указателю. Тот же приём, что у списка спрайтов шага A.
void test_no_pool() {
    GameplayEmitter e(nullptr, CAP, one_desc(), 2, 1u);
    check(e.burst(0, {}, 5) == 0 && e.count() == 0, "a pool that is not there holds nothing");
    check(e.dropped() == 5, "and every refused particle is counted");
}

// Таблицы описаний нет — отказ ТОЙ ЖЕ формы: вид частицы брать неоткуда.
void test_no_table() {
    GameplayEmitter e(pool, CAP, nullptr, 3, 1u);
    check(e.burst(0, {}, 4) == 0 && e.dropped() == 4, "no table means no particles");
}

// Описание вне таблицы — карта новее движка, а не порча: подача отказана и посчитана целиком.
void test_desc_outside_the_table() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    check(e.burst(7, {}, 3) == 0 && e.dropped() == 3, "a desc past the table refuses the burst");
    check(e.emit(7, {}, ONE) == 0 && e.dropped() == 4, "and the continuous source refuses too");
}

// Нулевое время жизни — отказ, а не частица: она умерла бы до первой отрисовки, и делённый на ноль
// возраст дал бы насыщение вместо цвета.
void test_zero_life() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    check(e.burst(1, {}, 6) == 0 && e.dropped() == 6, "a desc with no life spawns nothing");
}

// Потолок подачи: авторская запись с шестью нулями не крутит генератор миллион раз внутри тика.
// Лишнее посчитано потерей, а не отброшено молча.
void test_burst_ceiling() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    const uint32_t asked = MAX_BURST + 100;
    check(e.burst(0, {}, asked) == CAP, "the pool takes what it can hold");
    check(e.dropped() == asked - CAP, "and everything else is counted, ceiling included");
    // Числом потерь потолок НЕ подтверждается: пул на 64 частицы даёт тот же счёт и без него.
    // Подтверждается он ПОТОКОМ — тем самым, ради которого потолок и заведён: сверх него генератор
    // не крутится ни разу.
    GameplayEmitter exact(pool, CAP, one_desc(), 2, 1u);
    exact.burst(0, {}, MAX_BURST);
    check(e.stream() == exact.stream(), "and the ceiling really stopped the generator");
}

// Переполнение пула считается ПОШТУЧНО, и живых при этом ровно ёмкость.
void test_pool_overflow() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    check(e.burst(0, {}, CAP + 9) == CAP, "the pool fills to the brim");
    check(e.count() == CAP && e.dropped() == 9, "and the overflow is counted one by one");
}

// Непрерывный источник с нулевой ставкой не подаёт и НЕ ТЕРЯЕТ: «ничего не заказывали» и «заказали
// и не поместилось» обязаны быть различимы.
void test_zero_rate() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    for (uint32_t t = 0; t < 50; ++t) e.emit(0, {}, ONE);
    check(e.count() == 0 && e.dropped() == 0, "a rate of zero is silence, not loss");
}

// `clear` возвращает эмиттер в исходное, включая ДРОБНЫЙ ОСТАТОК непрерывного источника: остаток,
// переживший очистку, подал бы лишнюю частицу на первом же тике следующего уровня.
void test_clear_resets_everything() {
    EmitDesc d[1];
    d[0].life_ticks = 30;
    d[0].region = 1;
    d[0].rate_per_tick = fix32::from_float(0.75);
    GameplayEmitter e(pool, CAP, d, 1, 1u);
    e.emit(0, {}, ONE);
    e.burst(0, {}, CAP + 3);
    e.clear();
    check(e.count() == 0 && e.dropped() == 0, "clear drops the count and the loss");
    check(e.emit(0, {}, ONE) == 0, "and the fractional remainder went with them");
}

// Потеря при отрисовке принадлежит СПИСКУ, а не эмиттеру: у эмиттера частица жива, просто её некуда
// положить, и путать эти два счёта нельзя.
void test_draw_overflow_belongs_to_the_list() {
    GameplayEmitter e(pool, CAP, one_desc(), 2, 1u);
    e.burst(0, {}, 20);
    SpriteList list(sprites, keys, 8);
    check(e.draw(list) == 20, "the emitter offered every live particle");
    check(list.count() == 8 && list.dropped() == 12, "and the list counted what it could not take");
    check(e.dropped() == 0, "the emitter lost nothing of its own");
}

// Декоративный класс с нулевым шагом не стареет и не двигается: кадр, посчитанный дважды за один
// момент времени, не обязан стоить частицам жизни.
void test_zero_dt() {
    DecorEmitter e(pool, CAP, one_desc(), 2, 1u);
    e.burst(0, {ONE, ONE}, 3);
    const uint64_t before = scene::fold(e);
    for (int32_t i = 0; i < 10; ++i) e.advance(fix32{});
    check(before == scene::fold(e), "a zero step changes nothing");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("particle refusals\n");

    test_no_pool();
    test_no_table();
    test_desc_outside_the_table();
    test_zero_life();
    test_burst_ceiling();
    test_pool_overflow();
    test_zero_rate();
    test_clear_resets_everything();
    test_draw_overflow_belongs_to_the_list();
    test_zero_dt();

    std::printf("framework-graphics-particle-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
