#include <cstdio>

#include "framework_physics_stack_scene.hpp"
#include "platform_args.hpp"
#include "units.hpp"

// Гейт 3 спеки #15, вторая половина: та же башня, но СОБРАННАЯ падением.
//
// У поставленной башни накопленные импульсы растут с нуля в тишине, у собранной — приходят ударами,
// и тёплый старт получает вход, которого в первой сцене нет. Высоты (4, 6, 8, 10, 12, 14) выбраны
// так, чтобы удары приходили ВРАЗНОБОЙ, но ни один ящик не пролетал за кадр больше четверти своей
// высоты: сплошного контроля траектории у движка нет (CCD — follow-up спеки), и с большего разгона
// сцена мерила бы выталкивание из сквозного пролёта.
//
// Отдельной целью от `framework_physics_stack_test`: вопросы разные по существу, и имя упавшей цели
// в логе CI обязано отличать «башня не держится» от «башня не переживает удар». Сцена и мерки общие,
// из `framework_physics_stack_scene.hpp`, — иначе сравнение раскладок в конце сверяло бы две разные
// башни и оставалось бы зелёным при любом расхождении.
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
using namespace framework::physics::stack;

// По соглашению осей 1 юнит = 1 пиксель: башня, вставшая после падения в пределах пикселя от оси,
// встала «там же» — игрок сдвига не увидит.
constexpr fix32 PIXEL = fix32::from_int(1);

void test_tower_settles_after_impact() {
    World w(16);
    build(w, 4, 2);

    bool frozen_is_still = false;
    fix32 peak_depth{};
    const uint32_t at = settle(w, frozen_is_still, peak_depth);
    std::printf("  dropped: proven rest at frame %u, deepest contact on impact %d/65536\n", at,
                static_cast<int>(peak_depth.raw));

    check(at != 0, "a tower assembled by impact also comes to a proven stop");
    check(frozen_is_still, "and holds bit-identical once frozen");
    check(peak_depth < COLLAPSE, "without any box passing a quarter of its height into the next");
    check(!(CONTACT_BAND < deepest(w)), "landing with every contact inside the declared band");
    check(farthest_x(w) < PIXEL, "and within a pixel of the axis it fell along");

    // Раскладка покоя не зависит от того, КАК башня туда попала: два разных пути обязаны сойтись в
    // одну точку, и расхождение каждого контакта остаётся внутри его собственного допуска.
    //
    // Сверяются ПРОНИКНОВЕНИЯ, а не позиции ящиков, — по той же причине, по какой ими же мерится
    // дрейф (см. шапку `framework_physics_stack_scene.hpp`): позиция k-го ящика есть сумма k+1
    // проседаний под ним, и порог на неё пришлось бы назначать заново для каждой высоты. Прежняя
    // редакция применяла к этой сумме допуск ОДНОГО контакта, и держалось это на совпадении: подъём
    // числа итераций решателя с 8 до 16 просадил верхние контакты поставленной башни с 4132 и 4140
    // raw до 2356 и 328, у упавшей же они остались около 4100 — её проникновение заморожено сном
    // раньше, чем позиционная коррекция успела его выбрать. Поконтактно оба пути при этом сходятся:
    // худшая разница 3842 raw при допуске 4096, — то есть падало утверждение, которого файл не делал.
    World placed(16);
    build(placed, 0, 0);
    for (uint32_t frame = 1; frame <= REST_FRAMES_MAX; ++frame) placed.step(DT);
    for (uint32_t k = 0; k < BOXES; ++k) {
        const fix32 gap = abs_fix(depth(placed, k) - depth(w, k));
        check(gap < CONTACT_SLOP, "and with every contact of the chain settled to the same depth");
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics impact gate\n");
    test_tower_settles_after_impact();
    std::printf("framework-physics-impact: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
