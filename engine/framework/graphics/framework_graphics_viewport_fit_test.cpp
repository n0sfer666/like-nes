#include <cstdio>

#include "platform_args.hpp"
#include "viewport_fit.hpp"

// Политика вьюпорта (спека #24, В7): экраны из спеки и границы, на которых формула ломается
// первой, — смена `k` на пикселе, предел видимого, окно меньше зоны. Середина диапазона к
// округлению слепа: 1920×1080 даёт одно и то же число при floor, ceil и round.
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

bool same(PixelRect r, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

// Полосы и зона вместе обязаны замостить экран без зазора и нахлёста: HUD, положенный в полосу,
// иначе либо закрыл бы игру, либо оставил щель.
bool tiles_screen(const ViewportFit& f, PixelSize s) {
    uint64_t area = uint64_t{f.zone.w} * f.zone.h;
    for (uint32_t i = 0; i < f.strip_count; ++i) {
        const PixelRect r = f.strips[i];
        const bool inside_zone = r.x < f.zone.x + static_cast<int32_t>(f.zone.w) &&
                                 f.zone.x < r.x + static_cast<int32_t>(r.w) &&
                                 r.y < f.zone.y + static_cast<int32_t>(f.zone.h) &&
                                 f.zone.y < r.y + static_cast<int32_t>(r.h);
        if (inside_zone || r.w == 0 || r.h == 0) return false;
        area += uint64_t{r.w} * r.h;
    }
    return area == uint64_t{s.w} * s.h;
}

struct Case {
    PixelSize screen;
    uint32_t k;
    PixelSize visible;
    PixelRect zone;
    PixelRect shown;
    uint32_t strips;
    bool cropped;
    const char* what;
};

const Case CASES[] = {
    {{3440, 1440}, 6, {574, 240}, {568, 72, 2304, 1296}, {0, 0, 3440, 1440}, 4, false,
     "3440x1440: k = 6, 574x240 (ceil, not floor)"},
    {{2560, 1080}, 5, {512, 216}, {320, 0, 1920, 1080}, {0, 0, 2560, 1080}, 2, false,
     "2560x1080: k = 5, 512x216, strips left and right only"},
    {{1600, 1200}, 4, {400, 300}, {32, 168, 1536, 864}, {0, 0, 1600, 1200}, 4, false,
     "1600x1200: k = 4 by width, 400x300"},
    {{2532, 1170}, 5, {507, 234}, {306, 45, 1920, 1080}, {0, 0, 2532, 1170}, 4, false,
     "2532x1170: k = 5, 507x234"},
    {{1920, 1080}, 5, {384, 216}, {0, 0, 1920, 1080}, {0, 0, 1920, 1080}, 0, false,
     "1920x1080: the zone is the screen, no strips"},
    {{300, 200}, 1, {300, 200}, {0, 0, 300, 200}, {0, 0, 300, 200}, 0, true,
     "300x200: k = 1, the zone is cropped to the window"},
    {{800, 200}, 1, {576, 200}, {208, 0, 384, 200}, {112, 0, 576, 200}, 2, true,
     "800x200: a window wider than the zone but lower than it is still cropped"},
    {{5120, 1440}, 6, {576, 240}, {1408, 72, 2304, 1296}, {832, 0, 3456, 1440}, 4, false,
     "5120x1440: the visible width stops at the limit, the rest is fill"},
    {{767, 432}, 1, {576, 312}, {191, 108, 384, 216}, {95, 60, 576, 312}, 4, false,
     "767x432: one pixel short of k = 2 keeps k = 1, the limit binds both sides"},
    {{768, 432}, 2, {384, 216}, {0, 0, 768, 432}, {0, 0, 768, 432}, 0, false,
     "768x432: k = 2 exactly"},
    {{768, 431}, 1, {576, 312}, {192, 107, 384, 216}, {96, 59, 576, 312}, 4, false,
     "768x431: the height alone keeps k = 1"},
    {{1152, 624}, 2, {576, 312}, {192, 96, 768, 432}, {0, 0, 1152, 624}, 4, false,
     "1152x624: the visible area reaches the limit exactly"},
    {{1155, 627}, 2, {576, 312}, {193, 97, 768, 432}, {1, 1, 1152, 624}, 4, false,
     "1155x627: one ceil step past the limit leaves a fill pixel each side"},
    {{0, 0}, 1, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, 0, true,
     "0x0: a minimized window is cropped, not a division by zero"},
};

void test_cases() {
    for (const Case& c : CASES) {
        const ViewportFit f = viewport_fit(c.screen);
        const bool ok = f.scale == c.k && f.visible.w == c.visible.w && f.visible.h == c.visible.h &&
                        same(f.zone, c.zone.x, c.zone.y, c.zone.w, c.zone.h) &&
                        same(f.shown, c.shown.x, c.shown.y, c.shown.w, c.shown.h) &&
                        f.strip_count == c.strips && f.cropped == c.cropped &&
                        tiles_screen(f, c.screen);
        if (!ok)
            std::printf("  got k=%u visible=%ux%u zone=%d,%d %ux%u shown=%d,%d %ux%u strips=%u\n",
                        f.scale, f.visible.w, f.visible.h, f.zone.x, f.zone.y, f.zone.w, f.zone.h,
                        f.shown.x, f.shown.y, f.shown.w, f.shown.h, f.strip_count);
        check(ok, c.what);
    }
}

void test_view_follows_scale() {
    const ViewportFit f = viewport_fit({3440, 1440});
    check(f.view.zoom == fix32::from_int(6) && f.view.pixels_per_unit == 1 &&
              f.view.screen_half.x == fix32::from_int(1720) &&
              f.view.screen_half.y == fix32::from_int(720),
          "the view carries k as its zoom over the whole screen");
    check(viewport_is_pixel_exact(f.view), "an integer k keeps the world grid on the screen grid");
    const Vec2 half = viewport_half_world(f.view);
    check(half.x.raw * 2 <= fix32::from_int(static_cast<int32_t>(f.visible.w)).raw &&
              half.y.raw * 2 <= fix32::from_int(static_cast<int32_t>(f.visible.h)).raw,
          "where the visible area fills the screen, the drawn half stays inside it");
    // За пределом вид рисует дальше покрытого бейком: заливка вне `shown` здесь обязательна.
    const ViewportFit wide = viewport_fit({5120, 1440});
    check(viewport_half_world(wide.view).x.raw * 2 >
              fix32::from_int(static_cast<int32_t>(wide.visible.w)).raw,
          "past the limit the view draws beyond the covered area, so the fill is not optional");
    const Vec2 zone_half = view_zone_half();
    check(zone_half.x == fix32::from_int(192) && zone_half.y == fix32::from_int(108),
          "the camera clamps by half the zone, whatever the window");
}

void test_zone_argument() {
    check(viewport_fit({1920, 1080}, {0, 216}).scale == 0, "a zone with a zero side is refused");
    const ViewportFit big = viewport_fit({1280, 720}, {640, 360});
    check(big.scale == 2 && big.visible.w == 640 && big.visible.h == 360,
          "a zone wider than the limit raises the limit instead of cutting the zone");
    const ViewportFit small = viewport_fit({1920, 1080}, {320, 180});
    check(small.scale == 6 && small.visible.w == 320 && small.visible.h == 180,
          "the zone argument, not the default, picks k");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("viewport fit: integer scale, zone, limit, strips\n");

    test_cases();
    test_view_follows_scale();
    test_zone_argument();

    std::printf("framework-graphics-viewport-fit: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
