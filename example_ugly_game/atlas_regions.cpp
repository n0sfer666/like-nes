#include "atlas_regions.hpp"

namespace game {
namespace {

using framework::graphics::AtlasRegion;
using framework::graphics::AtlasTable;

// Имена выписаны СПИСКОМ, а не собраны из префикса и счётчика: собранное имя читается только вместе
// с кодом, который его собирает, а сверять номер с полем приходится глазами. Порядок здесь и
// порядок в `RegionSlot` — одно утверждение, и гейт проверяет именно его.
const char* const NAMES[] = {
    "ship",     "star",     "enemy",    "bullet",   "boss",     "hostile",  "solid",
    "digit_0",  "digit_1",  "digit_2",  "digit_3",  "digit_4",  "digit_5",  "digit_6",
    "digit_7",  "digit_8",  "digit_9",  "letter_a", "letter_b", "letter_c", "letter_d",
    "letter_e", "letter_f", "letter_g", "letter_h", "letter_i", "letter_j", "letter_k",
    "letter_l", "letter_m", "letter_n", "letter_o", "letter_p", "letter_q", "letter_r",
    "letter_s", "letter_t", "letter_u", "letter_v", "letter_w", "letter_x", "letter_y",
    "letter_z"};

static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == RID_Count - 1,
              "the name list and the region numbering are one statement");

// Одна реализация на константный и неконстантный доступ: вторая разошлась бы с первой ровно так же,
// как расходились две копии нарезки, — и разъезд был бы виден только на том спрайте, по которому
// попали.
template <class A>
auto slot(A& atlas, uint16_t id) -> decltype(&atlas.ship) {
    switch (id) {
        case RID_Ship: return &atlas.ship;
        case RID_Star: return &atlas.star;
        case RID_Enemy: return &atlas.enemy;
        case RID_Bullet: return &atlas.bullet;
        case RID_Boss: return &atlas.boss;
        case RID_Hostile: return &atlas.hostile;
        case RID_Solid: return &atlas.solid;
        default: break;
    }
    if (id >= RID_Digit0 && id < RID_LetterA) return &atlas.digit[id - RID_Digit0];
    if (id >= RID_LetterA && id < RID_Count) return &atlas.letter[id - RID_LetterA];
    return nullptr;
}

// Полтексела внутрь — приём САМПЛЕРА, и в таблице их нет намеренно (она говорит, где картинка
// лежит, а не как её фильтровать). Формула та же, что у процедурного `rgn`, но страница берётся
// у таблицы: два источника размера страницы разъехались бы ровно так же, как разъезжались UV.
Region uv(const AtlasRegion& r, float aw, float ah) {
    const float iu = 0.5f / aw, iv = 0.5f / ah;
    return {r.x / aw + iu, r.y / ah + iv, (r.x + r.w) / aw - iu, (r.y + r.h) / ah - iv};
}

bool one(const AtlasTable& table, const char* name, float aw, float ah, Region& out) {
    const std::optional<framework::graphics::RegionId> id = table.find(name);
    if (!id) return false;
    const std::optional<AtlasRegion> r = table.region(*id);
    if (!r) return false;
    out = uv(*r, aw, ah);
    return true;
}

} // namespace

const char* region_name(uint16_t id) {
    return (id == RID_None || id >= RID_Count) ? nullptr : NAMES[id - 1];
}

const Region* region_at(const Atlas& atlas, uint16_t id) { return slot(atlas, id); }

bool regions_from_table(const AtlasTable& table, Atlas& atlas) {
    if (!table.valid() || table.page_width() == 0 || table.page_height() == 0) return false;
    const float aw = static_cast<float>(table.page_width());
    const float ah = static_cast<float>(table.page_height());

    // Отказ ЦЕЛЫЙ: нарезка собирается в СВОЙ экземпляр и ложится поверх пикселей вызывающего только
    // целиком. Половина полей из таблицы и половина из кода была бы третьим источником, худшим из
    // трёх.
    Atlas got;
    for (uint16_t id = RID_None + 1; id < RID_Count; ++id) {
        Region* dst = slot(got, id);
        if (dst == nullptr || !one(table, region_name(id), aw, ah, *dst)) return false;
    }

    got.px = std::move(atlas.px);
    got.bc7 = std::move(atlas.bc7);
    got.w = table.page_width();
    got.h = table.page_height();
    atlas = std::move(got);
    return true;
}

} // namespace game
