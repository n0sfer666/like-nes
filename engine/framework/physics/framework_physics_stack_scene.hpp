#pragma once

#include "units.hpp"
#include "world.hpp"

// Сцена гейта 3 спеки #15 — башня из шести ящиков на полу — и мерки к ней, общие для ДВУХ целей:
// башня, поставленная впритык, и башня, собранная падением. Вопрос у них разный (устойчивость
// против переживания удара), а сцена обязана быть одной и той же: сравнение раскладок покоя,
// которым кончается вторая цель, сверяет её результат с первой, и разошедшиеся константы сделали бы
// это сравнение бессмысленным молча — оба прогона остались бы зелёными, сверяя разные башни.
//
// Мерится ПРОНИКНОВЕНИЕ КАЖДОГО КОНТАКТА цепи, а не смещение ящика от стартовой точки. Смещение
// верхнего ящика — сумма проседаний всех шести контактов под ним, то есть величина, растущая с
// высотой башни: порог на неё пришлось бы назначать заново для каждого K. Проникновение одного
// контакта ограничено тем, что движок УЖЕ объявил, — полосой `[-SPECULATIVE_MARGIN, CONTACT_SLOP]`
// (`units.hpp`), и порог взят оттуда, а не подобран по прогону.
namespace framework::physics::stack {

constexpr fix32 DT = fix32::from_float(1.0 / 60.0);
constexpr uint32_t BOXES = 6;
constexpr fix32 HALF = fix32::from_int(8);
constexpr fix32 FLOOR_TOP = fix32::from_int(192);
constexpr uint32_t REST_FRAMES_MAX = 30 * 60;

// Полоса покоящегося контакта — ровно та, что объявлена в `units.hpp`: снизу спекулятивное поле
// (контакт живёт уже при зазоре), сверху допуск, ниже которого коррекция не выталкивает.
constexpr fix32 CONTACT_BAND = CONTACT_SLOP + SPECULATIVE_MARGIN;
// «Взрыв»: ящик, влезший в соседа на четверть собственной высоты, — уже не проседание, а провал.
// Величина геометрическая, из размера тела, и к допускам решателя отношения не имеет.
constexpr fix32 COLLAPSE = fix32::from_int(4);

inline fix32 rest_y(uint32_t i) {
    return FLOOR_TOP - HALF - fix32::from_int(static_cast<int32_t>(i) * 16);
}

// При `base` = 0 башня ставится ВПРИТЫК: зазор дал бы падение, то есть удар, и гейт мерил бы
// затухание удара вместо устойчивости; нахлёст дал бы стартовое выталкивание.
inline void build(World& w, int32_t base, int32_t step) {
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
inline fix32 depth(const World& w, uint32_t k) {
    const fix32 upper = w.bodies()[k + 1].position.y;
    const fix32 lower = k == 0 ? FLOOR_TOP : w.bodies()[k].position.y - HALF;
    return (upper + HALF) - lower;
}

inline fix32 deepest(const World& w) {
    fix32 worst = depth(w, 0);
    for (uint32_t k = 1; k < BOXES; ++k) worst = max_fix(worst, depth(w, k));
    return worst;
}

inline fix32 farthest_x(const World& w) {
    fix32 worst{};
    for (uint32_t i = 0; i < BOXES; ++i) worst = max_fix(worst, abs_fix(w.bodies()[i + 1].position.x));
    return worst;
}

inline bool all_at_rest(const World& w) {
    for (uint32_t i = 0; i < BOXES; ++i) {
        if (!w.at_rest(BodyId{i + 1})) return false;
    }
    return true;
}

// Прогон до доказанного покоя. Возвращает кадр замирания (0 — не замерла) и по дороге проверяет, что
// ЗАМЕРШЕЕ действительно стоит: с кадра замирания и до конца окна хеш состояния обязан быть тем же
// битом. Это единственное утверждение гейта без порога вовсе — и потому самое сильное.
inline uint32_t settle(World& w, bool& frozen_is_still, fix32& peak_depth) {
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

} // namespace framework::physics::stack
