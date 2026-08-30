#include <cstdio>

#include "platform_args.hpp"
#include "viewport.hpp"

// Отказы вида (вертикаль 2, шаг D спеки #17). Зум приходит из настроек игрока и из скрипта
// катсцены, то есть снаружи, и вырожденное значение здесь ловится не для красоты: деление на ноль
// в fix32 НАСЫЩАЕТ, поэтому нулевой зум не падает, а рисует мир одним пикселем — это выглядит как
// чёрный экран, а не как неверная настройка, и разбираться пришлось бы с бэкендом.
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

Viewport ok_view() {
    Viewport v;
    v.screen_half = {fix32::from_int(320), fix32::from_int(180)};
    v.zoom = fix32::from_int(2);
    v.pixels_per_unit = 4;
    return v;
}

void expect(Viewport v, ViewportFault want, const char* what) {
    check(viewport_check(v) == want, what);
    if (want == VIEWPORT_OK) return;
    // Отказ обязан быть НЕ ТОЛЬКО НАЗВАН: вид, признанный негодным, не отдаёт ни масштаба, ни
    // координат — иначе назвавший отказ остался бы единственным, кто про него знает.
    check(viewport_scale(v).raw == 0, "a refused viewport has no scale");
    check(viewport_half_world(v) == Vec2{}, "a refused viewport shows nothing");
    check(world_to_screen(v, Vec2{}, {fix32::from_int(9), fix32::from_int(9)}) == Vec2{},
          "a refused viewport maps nothing to the screen");
    check(screen_to_world(v, Vec2{}, {fix32::from_int(9), fix32::from_int(9)}) == Vec2{},
          "a refused viewport maps nothing back to the world");
    check(!viewport_is_pixel_exact(v), "a refused viewport is not a grid either");
}

void test_the_healthy_viewport_is_healthy() {
    // Позитивный контроль: набор, где ВСЁ отбито, зелен ровно так же, как честный.
    expect(ok_view(), VIEWPORT_OK, "a filled-in viewport is accepted");
    check(viewport_scale(ok_view()) == fix32::from_int(8), "control: the accepted viewport works");
}

void test_zoom_must_be_positive() {
    Viewport zero = ok_view();
    zero.zoom = fix32{};
    expect(zero, VIEWPORT_ZOOM_NOT_POSITIVE, "zero zoom is named, not divided by");
    Viewport neg = ok_view();
    neg.zoom = fix32::from_int(-2);
    expect(neg, VIEWPORT_ZOOM_NOT_POSITIVE, "negative zoom is named, not mirrored");
}

void test_pixels_per_unit_must_be_positive() {
    Viewport zero = ok_view();
    zero.pixels_per_unit = 0;
    expect(zero, VIEWPORT_PPU_NOT_POSITIVE, "zero pixels-per-unit is named");
    Viewport neg = ok_view();
    neg.pixels_per_unit = -4;
    expect(neg, VIEWPORT_PPU_NOT_POSITIVE, "negative pixels-per-unit is named");
}

void test_the_screen_must_have_area() {
    Viewport flat = ok_view();
    flat.screen_half.y = fix32{};
    expect(flat, VIEWPORT_SCREEN_EMPTY, "a screen with no height is named");
    Viewport thin = ok_view();
    thin.screen_half.x = fix32::from_int(-1);
    expect(thin, VIEWPORT_SCREEN_EMPTY, "a screen with negative width is named");
    // Обе оси проверяются, а не одна: свёрнутое окно приходит из ресайза, и «высота нулевая, а
    // ширина нет» — самый обычный его кадр.
    Vec2 half = flat.screen_half;
    check(half.x.raw > 0, "control: the flat screen really was flat in one axis only");
}

void test_the_defaults_are_a_viewport_without_a_screen() {
    // Значения по умолчанию — тоже контракт: `zoom = 1` значит «без зума», `ppu = 1` — «пиксель на
    // мировую единицу». Не проверь их — поле, поставленное мимо, всплыло бы у первого потребителя,
    // забывшего заполнить структуру целиком, и выглядело бы как неверный зум, а не как умолчание.
    const Viewport bare;
    check(bare.zoom == fix32::from_int(1), "the default zoom is one");
    check(bare.pixels_per_unit == 1, "the default is one pixel per world unit");
    check(viewport_check(bare) == VIEWPORT_SCREEN_EMPTY,
          "a default viewport lacks only its screen size");
}

void test_the_first_named_fault_is_the_one_to_fix() {
    // Испорчено всё сразу — назван обязан быть ОДИН и тот же отказ, и первым стоит
    // `pixels_per_unit`: это свойство СБОРКИ (масштаб рисунка), а зум — свойство момента, и при
    // неверном ppu вопрос про зум не имеет смысла вовсе. Порядок здесь контракт, а не совпадение:
    // отказ читает человек, и «то ppu, то зум на одной и той же конфигурации» — плохой отчёт.
    Viewport broken;
    broken.zoom = fix32::from_int(-1);
    broken.pixels_per_unit = 0;
    broken.screen_half = Vec2{};
    check(viewport_check(broken) == VIEWPORT_PPU_NOT_POSITIVE,
          "the build-time fault is named before the moment-to-moment one");
}

void test_the_smallest_zoom_still_works() {
    // Граница между «мелко» и «отказ» проходит по нулю, а не по вкусу: один сырой шаг Q16.16 — уже
    // рабочий зум, и отбивать его значило бы завести второй, необъявленный порог.
    Viewport tiny = ok_view();
    tiny.zoom = fix32::from_raw(1);
    expect(tiny, VIEWPORT_OK, "the smallest positive zoom is accepted");
    check(viewport_scale(tiny).raw == 4, "control: the smallest zoom really scales by ppu");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args utf8_argv(argc, argv);
    std::printf("viewport refusals\n");

    test_the_healthy_viewport_is_healthy();
    test_zoom_must_be_positive();
    test_pixels_per_unit_must_be_positive();
    test_the_screen_must_have_area();
    test_the_defaults_are_a_viewport_without_a_screen();
    test_the_first_named_fault_is_the_one_to_fix();
    test_the_smallest_zoom_still_works();

    std::printf("framework-graphics-viewport-refusal: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
