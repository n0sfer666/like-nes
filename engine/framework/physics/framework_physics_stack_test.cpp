#include <cstdio>

#include "platform_args.hpp"
#include "units.hpp"
#include "world.hpp"

// Гейт 3 спеки #15: башня из шести ящиков стоит десять секунд без дрейфа и без «взрыва».
//
// Численный критерий, а не «на глаз», и мерится он ПРОНИКНОВЕНИЕМ КАЖДОГО КОНТАКТА цепи, а не
// смещением ящика от стартовой точки. Смещение верхнего ящика — сумма проседаний всех шести контактов
// под ним, то есть величина, растущая с высотой башни: порог на неё пришлось бы назначать заново для
// каждого K. Проникновение одного контакта ограничено тем, что движок УЖЕ объявил, — полосой
// `[-SPECULATIVE_MARGIN, CONTACT_SLOP]` (`units.hpp`), и порог взят оттуда, а не подобран по прогону.
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

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr uint32_t BOXES = 6;
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr uint32_t DRIFT_FRAMES = 10 * 60;
constexpr uint32_t REST_FRAMES_MAX = 30 * 60;
// Кадр, к которому загрузка цепи заведомо кончилась: импульс доходит до низа не мгновенно, первые
// секунды башня СЖИМАЕТСЯ. Число нужно только для сравнения «раньше против позже».
constexpr uint32_t LOADED_AT = 60;

// Полоса покоящегося контакта — ровно та, что объявлена в `units.hpp`: снизу спекулятивное поле
// (контакт живёт уже при зазоре), сверху допуск, ниже которого коррекция не выталкивает.
constexpr fix32 CONTACT_BAND = CONTACT_SLOP + SPECULATIVE_MARGIN;
// «Взрыв»: ящик, влезший в соседа на четверть собственной высоты, — уже не проседание, а провал.
// Величина геометрическая, из размера тела, и к допускам решателя отношения не имеет.
constexpr fix32 COLLAPSE = fix32::from_int(4);
// По соглашению осей 1 юнит = 1 пиксель: башня, вставшая после падения в пределах пикселя от оси,
// встала «там же» — игрок сдвига не увидит.
constexpr fix32 PIXEL = fix32::from_int(1);

fix32 rest_y(uint32_t i) {
    return FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16);
}

// При `base` = 0 башня ставится ВПРИТЫК: зазор дал бы падение, то есть удар, и гейт мерил бы
// затухание удара вместо устойчивости; нахлёст дал бы стартовое выталкивание.
void build(World& w, int32_t base, int32_t step) {
    BodyDesc floor;
    floor.key = 1;
    floor.type = BodyType::Static;
    floor.shape = box(fix32::from_int(128), fix32::from_int(8));
    floor.position = {fix32{}, fix32::from_int(200)};
    floor.material = {fix32{}, fix32::from_float(0.6)};
    w.add(floor);

    for (uint32_t i = 0; i < BOXES; ++i) {
        BodyDesc b;
        b.key = 10 + i;
        b.shape = box(HALF, HALF);
        b.position = {fix32{}, rest_y(i) - fix32::from_int(base + step * static_cast<int32_t>(i))};
        b.mass = fix32::from_int(4);
        b.material = {fix32{}, fix32::from_float(0.6)};
        w.add(b);
    }
}

// Проникновение k-го контакта цепи: нулевой — нижний ящик о пол, k-й — ящик k о ящик k-1.
// Положительное — влезли друг в друга, отрицательное — между ними зазор.
fix32 depth(const World& w, uint32_t k) {
    const fix32 upper = w.bodies()[k + 1].position.y;
    const fix32 lower = k == 0 ? FLOOR_TOP : w.bodies()[k].position.y - HALF;
    return (upper + HALF) - lower;
}

fix32 deepest(const World& w) {
    fix32 worst = depth(w, 0);
    for (uint32_t k = 1; k < BOXES; ++k) worst = max_fix(worst, depth(w, k));
    return worst;
}

fix32 farthest_x(const World& w) {
    fix32 worst{};
    for (uint32_t i = 0; i < BOXES; ++i) worst = max_fix(worst, abs_fix(w.bodies()[i + 1].position.x));
    return worst;
}

bool all_at_rest(const World& w) {
    for (uint32_t i = 0; i < BOXES; ++i) {
        if (!w.at_rest(BodyId{i + 1})) return false;
    }
    return true;
}

// Прогон до доказанного покоя. Возвращает кадр замирания (0 — не замерла) и по дороге проверяет, что
// ЗАМЕРШЕЕ действительно стоит: с кадра замирания и до конца окна хеш состояния обязан быть тем же
// битом. Это единственное утверждение гейта без порога вовсе — и потому самое сильное.
uint32_t settle(World& w, bool& frozen_is_still, fix32& peak_depth) {
    uint32_t at = 0;
    uint64_t frozen_hash = 0;
    frozen_is_still = true;
    for (uint32_t frame = 1; frame <= REST_FRAMES_MAX; ++frame) {
        w.step(DT);
        peak_depth = max_fix(peak_depth, deepest(w));
        if (at == 0 && all_at_rest(w)) {
            at = frame;
            frozen_hash = w.hash();
        } else if (at != 0 && w.hash() != frozen_hash) {
            frozen_is_still = false;
        }
    }
    return at;
}

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

// Тот же вопрос, но башня СОБИРАЕТСЯ падением: у поставленной накопленные импульсы растут с нуля в
// тишине, у собранной — приходят ударами, и тёплый старт получает вход, которого в первой сцене нет.
// Высоты (4, 6, 8, 10, 12, 14) выбраны так, чтобы удары приходили ВРАЗНОБОЙ, но ни один ящик не
// пролетал за кадр больше четверти своей высоты: сплошного контроля траектории у движка нет (CCD —
// follow-up спеки), и с большего разгона сцена мерила бы выталкивание из сквозного пролёта.
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
    // одну точку, и расхождение остаётся внутри допуска ОДНОГО контакта, хотя копилось через шесть.
    World placed(16);
    build(placed, 0, 0);
    for (uint32_t frame = 1; frame <= REST_FRAMES_MAX; ++frame) placed.step(DT);
    for (uint32_t i = 0; i < BOXES; ++i) {
        const fix32 dy = abs_fix(placed.bodies()[i + 1].position.y - w.bodies()[i + 1].position.y);
        check(dy < CONTACT_SLOP, "and in the same layout as the tower placed by hand");
    }
}

} // namespace

int main(int argc, char** argv) {
    platform::Args args(argc, argv);
    std::printf("framework physics stack gate\n");
    test_tower_stands();
    test_tower_settles_after_impact();
    std::printf("framework-physics-stack: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
