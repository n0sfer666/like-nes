#include <cstdio>

#include "framework_physics_stack_scene.hpp"
#include "platform_args.hpp"
#include "units.hpp"

// Гейт 3 спеки #15, первая половина: башня из шести ящиков, поставленная ВПРИТЫК, стоит десять
// секунд без дрейфа и без «взрыва», а потом приходит к доказанному покою.
//
// Половина отдельной целью, а не случаем в общей: у поставленной башни накопленные импульсы растут с
// нуля в тишине, у собранной падением — приходят ударами, и это два разных входа тёплого старта.
// Имя упавшей цели в логе CI обязано называть сломанный, а «стопка сломана» не называет ничего.
// Сцена и мерки — в `framework_physics_stack_scene.hpp`, там же обоснование порогов.
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

constexpr uint32_t DRIFT_FRAMES = 10 * 60;
// Кадр, к которому загрузка цепи заведомо кончилась: импульс доходит до низа не мгновенно, первые
// секунды башня СЖИМАЕТСЯ. Число нужно только для сравнения «раньше против позже».
constexpr uint32_t LOADED_AT = 60;

void test_tower_stands() {
    World w(16);
    build(w, 0, 0);

    fix32 worst_x{};
    fix32 peak_depth{};
    fix32 loaded_depth{};
    for (uint32_t frame = 1; frame <= DRIFT_FRAMES; ++frame) {
        w.step(DT);
        worst_x = max_fix(worst_x, farthest_x(w));
        peak_depth = max_fix(peak_depth, deepest(w));
        if (frame == LOADED_AT) loaded_depth = deepest(w);
    }
    const fix32 settled_depth = deepest(w);
    std::printf("  placed: worst x=%d/65536, deepest contact %d -> %d/65536\n",
                static_cast<int>(worst_x.raw), static_cast<int>(loaded_depth.raw),
                static_cast<int>(settled_depth.raw));

    // Вбок башне ехать не с чего: сцена симметрична, и весь боковой ход — это округление решателя.
    // Порог — допуск ОДНОГО контакта, самая мелкая величина, которую движок вообще объявляет.
    check(worst_x < CONTACT_SLOP, "a stack of boxes never drifts sideways");
    check(peak_depth < COLLAPSE, "and no box ever sinks a quarter of its own height into the next");
    // Утверждение БЕЗ ПОРОГА: за десять секунд цепь обязана распрямляться, а не проседать. Медленное
    // расползание отличается от устойчивости только знаком этой разности.
    check(settled_depth < loaded_depth, "the chain relaxes as the run goes on instead of sinking");
    check(!(CONTACT_BAND < settled_depth), "and every contact rests inside the declared contact band");

    bool frozen_is_still = false;
    fix32 tail_depth{};
    const uint32_t at = settle(w, frozen_is_still, tail_depth);
    std::printf("  placed: proven rest at frame %u\n", DRIFT_FRAMES + at);
    // Доказанный покой — вторая половина гейта: «взрыв» и медленное расползание отличаются только
    // скоростью, и башня, ни разу не пришедшая к неподвижной точке, дрейфует — просто медленнее порога.
    check(at != 0, "and comes to a proven stop rather than creeping below the threshold");
    check(frozen_is_still, "with the frozen state bit-identical for the rest of the run");
    check(!(CONTACT_BAND < tail_depth), "and never re-sinks past the band while getting there");
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics stack gate\n");
    test_tower_stands();
    std::printf("framework-physics-stack: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
