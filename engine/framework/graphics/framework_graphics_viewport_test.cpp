#include <cstdio>

#include "hash_mix.hpp"
#include "platform_args.hpp"
#include "viewport.hpp"

// Вертикаль 2, шаг D спеки #17: зум. Предмет здесь не «картинка стала крупнее» — картинки на
// раннере нет вовсе, — а СОГЛАШЕНИЕ о числе экранных пикселей на мировую единицу и его следствия:
// видимая область, окно culling'а, границы уровня и пиксельная сетка.
//
// Целей у шага две, и граница между ними — класс отказа. Здесь ПЕРЕВОД КООРДИНАТ и пиксельная
// сетка: «зум посчитан неверно» видно по числу. До кого зум обязан доехать — culling и границы
// уровня — спрашивает `..._zoom_reach_test`: там отказ выглядит как правильная картинка в неверном
// окне, и в одном файле он утонул бы. Голден в конце отвечает «три машины сошлись» и за верность не
// отвечает — за неё отвечают утверждения выше него.
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

Viewport make(fix32 zoom, int32_t ppu) {
    Viewport v;
    v.screen_half = {fix32::from_int(320), fix32::from_int(180)};
    v.zoom = zoom;
    v.pixels_per_unit = ppu;
    return v;
}

void test_scale_is_one_number() {
    const Viewport v = make(fix32::from_int(2), 4);
    check(viewport_scale(v) == fix32::from_int(8), "scale is pixels-per-unit times zoom");
    const Vec2 half = viewport_half_world(v);
    check(half.x == fix32::from_int(40) && half.y == fix32::from_float(22.5),
          "the visible half follows the scale");
    // Зум ВДВОЕ обязан ровно вдвое сузить видимое: иначе «зум» и «масштаб» разошлись бы в числах,
    // оставшись одним словом.
    const Vec2 wide = viewport_half_world(make(fix32::from_int(1), 4));
    check(wide.x == half.x * fix32::from_int(2) && wide.y == half.y * fix32::from_int(2),
          "zooming out by two widens the visible half by exactly two");
}

void test_round_trip() {
    const Viewport exact = make(fix32::from_int(3), 2);
    const Vec2 c = {fix32::from_int(17), fix32::from_int(-9)};
    const Vec2 w = {fix32::from_float(21.25), fix32::from_float(-3.5)};
    const Vec2 back = screen_to_world(exact, c, world_to_screen(exact, c, w));
    check(back == w, "an integer scale maps world to screen and back exactly");
    // Дробный масштаб точным быть не обязан — деление в Q16.16 усекается, — но обязан быть
    // ОГРАНИЧЕННЫМ: «в пределах пикселя» это утверждение, а «как получится» — нет.
    const Viewport frac = make(fix32::from_float(1.7), 3);
    const Vec2 loose = screen_to_world(frac, c, world_to_screen(frac, c, w));
    const fix32 err = abs_fix(loose.x - w.x) + abs_fix(loose.y - w.y);
    check(err.raw < fix32::ONE / 64, "a fractional scale round-trips within a fraction of a unit");
    // Центр вида — экранная середина при любом зуме: это и есть определение центра.
    check(world_to_screen(frac, c, c) == frac.screen_half, "the centre lands on the screen centre");
}



void test_pixel_snap_does_not_jitter() {
    const Viewport v = make(fix32::from_int(2), 4);
    // Центр вида смещён так, чтобы ход прошёл ЧЕРЕЗ НОЛЬ экранной координаты. Ход по одним
    // положительным пикселям не отличает округление вниз от усечения к нулю, а усечение — это ровно
    // то дрожание, которое режим обязан убрать: слева от нуля картинка дёргается назад на пиксель.
    const Vec2 c = {fix32::from_int(40), fix32{}};
    const Vec2 origin = {fix32::from_raw(-33 * (fix32::ONE / 32)), fix32{}};
    int32_t prev = world_to_screen_snapped(v, c, origin).x.to_int();
    int32_t steps = 0;
    // Субпиксельный ход: за 64 шага по 1/32 мировой единицы экран обязан двигаться ТОЛЬКО ВПЕРЁД и
    // ровно на пиксель за раз. Дрожание — это шаг назад, и без движения вперёд его не отличить от
    // неподвижной картинки, поэтому считается и то, и другое.
    for (int32_t i = -32; i < 32; ++i) {
        const Vec2 w = {fix32::from_raw(i * (fix32::ONE / 32)), fix32{}};
        const int32_t now = world_to_screen_snapped(v, c, w).x.to_int();
        check(now >= prev, "a snapped screen position never moves backwards");
        check(now - prev <= 1, "a snapped screen position never skips a pixel");
        if (now != prev) ++steps;
        prev = now;
    }
    std::printf("  snap: 64 substeps advanced %d pixels\n", steps);
    check(steps == 16, "control: the substeps really did advance the picture");
    check(viewport_is_pixel_exact(v), "an integer scale keeps the world grid on the screen grid");
    check(!viewport_is_pixel_exact(make(fix32::from_float(1.5), 3)),
          "a fractional scale says so instead of pretending to be a grid");
}

uint64_t golden(int32_t nudge) {
    uint64_t h = physics::FNV_OFFSET;
    const fix32 zooms[5] = {fix32::from_float(0.25), fix32::from_float(0.5), fix32::from_int(1),
                            fix32::from_float(1.5), fix32::from_int(4)};
    for (uint32_t z = 0; z < 5; ++z) {
        for (int32_t ppu = 1; ppu <= 4; ++ppu) {
            const Viewport v = make(zooms[z] + fix32::from_raw(nudge), ppu);
            const Vec2 c = {fix32::from_int(static_cast<int32_t>(z) * 37 - 40),
                            fix32::from_int(ppu * 13)};
            const Vec2 half = viewport_half_world(v);
            physics::mix(h, static_cast<uint32_t>(viewport_scale(v).raw));
            physics::mix(h, static_cast<uint32_t>(half.x.raw));
            physics::mix(h, static_cast<uint32_t>(half.y.raw));
            physics::mix(h, viewport_is_pixel_exact(v) ? 1u : 0u);
            for (int32_t k = -3; k <= 3; ++k) {
                const Vec2 w = {c.x + fix32::from_raw(k * (fix32::ONE / 3)),
                                c.y - fix32::from_raw(k * (fix32::ONE / 7))};
                const Vec2 s = world_to_screen(v, c, w);
                const Vec2 snapped = world_to_screen_snapped(v, c, w);
                physics::mix(h, static_cast<uint32_t>(s.x.raw));
                physics::mix(h, static_cast<uint32_t>(s.y.raw));
                physics::mix(h, static_cast<uint32_t>(snapped.x.raw));
                physics::mix(h, static_cast<uint32_t>(snapped.y.raw));
                physics::mix(h, static_cast<uint32_t>(screen_to_world(v, c, s).x.raw));
            }
        }
    }
    return h;
}

void test_golden() {
    const uint64_t h = golden(0);
    std::printf("  viewport hash = 0x%016llx\n", static_cast<unsigned long long>(h));
    check(h == 0x5bcea5ecad651b07ull, "the viewport golden matches");
    // Свёртка обязана ЧИТАТЬ зум, а не пересчитывать константу: сдвиг на ОДИН сырой шаг Q16.16 —
    // самое малое, чем зум вообще может отличаться, — обязан её изменить.
    check(golden(1) != h, "control: the fold really reads the zoom");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("camera zoom: world to screen\n");

    test_scale_is_one_number();
    test_round_trip();
    test_pixel_snap_does_not_jitter();
    test_golden();

    std::printf("framework-graphics-viewport: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
