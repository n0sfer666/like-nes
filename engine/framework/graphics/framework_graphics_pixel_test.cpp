#include <cstdio>

#include "camera.hpp"
#include "platform_args.hpp"

// Гейт ПИКСЕЛЬНОЙ СЕТКИ И ПАРАЛЛАКСА (шаг C вертикали 1, гейт 5 спеки #17): привязка округлением
// ВНИЗ и слои, едущие своей долей.
//
// Оба утверждения про одно — про дрожание относительно соседа. Усечение к нулю удваивает пиксель
// на нуле, привязка до умножения на долю снимает фон с сетки: ни то, ни другое на позиции камеры
// не видно, и оба видны только на ходу.
namespace {

using namespace framework;
using namespace framework::graphics;

int fails = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what);
        ++fails;
    }
}

void same(fix32 got, fix32 want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %.6f (raw %d), want %.6f (raw %d)\n", what, got.to_double(),
                got.raw, want.to_double(), want.raw);
    ++fails;
}

void same_u(uint32_t got, uint32_t want, const char* what) {
    if (got == want) return;
    std::printf("  FAIL: %s: got %u, want %u\n", what, got, want);
    ++fails;
}

fix32 fx(int32_t v) { return fix32::from_int(v); }

// 4. Пиксельная сетка: значение округляется ВНИЗ, поэтому пиксель у нуля ровно такой же ширины, как
// любой другой. Усечение к нулю делает его вдвое шире — камера, проезжающая через ноль, дёргается
// там на целый пиксель, и это единственное место, где дефект виден.
void test_pixel_grid() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_PIXEL_PERFECT;
    cfg.pixels_per_unit = 8;
    const int32_t step = fix32::ONE / 8;

    Camera c{};
    uint32_t at_zero = 0, drops = 0, off_grid = 0;
    fix32 prev = fix32::from_raw(-2 * step);
    for (int32_t raw = -step * 2; raw <= step * 2; ++raw) {
        c.center.x = fix32::from_raw(raw);
        const fix32 got = camera_view_center(c, cfg, 0).x;
        if (got.raw == 0) ++at_zero;
        if (got < prev) ++drops;
        if (got.raw % step != 0) ++off_grid;
        prev = got;
    }
    same_u(at_zero, static_cast<uint32_t>(step), "the pixel at zero is exactly one pixel wide");
    same_u(drops, 0, "the snapped centre never moves backwards while the camera moves forward");
    same_u(off_grid, 0, "and every snapped value sits on the grid");

    CameraConfig off{};
    Camera raw_cam{};
    raw_cam.center.x = fix32::from_raw(step / 2);
    same(camera_view_center(raw_cam, off, 0).x, fix32::from_raw(step / 2),
         "without the policy the sub-pixel position is passed through untouched");
}

// 5. Параллакс: слой едет за камерой СВОЕЙ долей, трясётся той же долей, и на сетку садится ПОСЛЕ
// умножения. Привязка до умножения выглядит так же на доле единица и разъезжается на всех
// остальных — фон дрожал бы относительно переднего плана ровно там, где режим это убирает.
void test_parallax() {
    CameraConfig cfg{};
    cfg.policies = CAMERA_PIXEL_PERFECT;
    cfg.pixels_per_unit = 4;
    const int32_t step = fix32::ONE / 4;
    const fix32 half = fix32::from_raw(fix32::ONE / 2);

    Camera c{};
    c.center = Vec2{fx(40), fx(24)};
    same(camera_layer_center(c, cfg, 0, fx(1)).x, camera_view_center(c, cfg, 0).x,
         "a layer at factor one is the game layer");
    same(camera_layer_center(c, cfg, 0, half).x, fx(20), "a layer at one half moves half as far");
    same(camera_layer_center(c, cfg, 0, fix32{}).x, fx(0), "a layer at zero does not move at all");

    // Доля, кратная сетке НЕ дающая: привязанное значение, умноженное на 3/8, попадает на сетку
    // лишь на каждом восьмом пикселе. Шаг петли берётся БОЛЬШЕ пикселя намеренно: пройди она
    // внутри одного, все четыреста положений привязались бы к одному значению, и утверждение
    // «всё на сетке» стало бы правдой про единственную точку.
    const fix32 third = fix32::from_raw(fix32::ONE * 3 / 8);
    uint32_t off_grid = 0, buckets = 0;
    int32_t seen = -1;
    for (int32_t raw = 0; raw < 400; ++raw) {
        c.center.x = fix32::from_raw(raw * 12289);
        const int32_t got = camera_layer_center(c, cfg, 0, third).x.raw;
        if (got % step != 0) ++off_grid;
        if (got != seen) { ++buckets; seen = got; }
    }
    same_u(off_grid, 0, "every layer position sits on the pixel grid, snapped AFTER the factor");
    check(buckets > 50, "and the sweep actually crosses pixels instead of standing in one");

    // «Не шире» выполняется и для тряски, не умноженной на долю вовсе: у камеры в нуле оба слоя
    // тогда трясутся ОДИНАКОВО, а равенство проходит нестрогое сравнение. Поэтому утверждений два.
    Camera shaken{};
    camera_shake(shaken, 60, fx(8), 77);
    uint32_t wider = 0, strictly = 0;
    for (uint64_t t = 0; t < 40; ++t) {
        const fix32 near_ = abs_fix(camera_layer_center(shaken, cfg, t, fx(1)).x);
        const fix32 far_ = abs_fix(camera_layer_center(shaken, cfg, t, half).x);
        if (near_.raw < far_.raw) ++wider;
        if (far_.raw < near_.raw) ++strictly;
    }
    same_u(wider, 0, "a distant layer never shakes wider than the game layer");
    check(strictly > 25, "and on almost every tick it shakes strictly less");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("graphics camera pixel grid and parallax\n");
    test_pixel_grid();
    test_parallax();
    std::printf("framework-graphics-pixel: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
