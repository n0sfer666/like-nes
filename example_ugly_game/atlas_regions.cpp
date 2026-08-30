#include "atlas_regions.hpp"

namespace game {
namespace {

using framework::graphics::AtlasRegion;
using framework::graphics::AtlasTable;

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

bool regions_from_table(const AtlasTable& table, Atlas& atlas) {
    if (!table.valid() || table.page_width() == 0 || table.page_height() == 0) return false;
    const float aw = static_cast<float>(table.page_width());
    const float ah = static_cast<float>(table.page_height());

    Atlas got;
    bool ok = one(table, "ship", aw, ah, got.ship) && one(table, "star", aw, ah, got.star) &&
              one(table, "enemy", aw, ah, got.enemy) && one(table, "bullet", aw, ah, got.bullet) &&
              one(table, "hostile", aw, ah, got.hostile) && one(table, "boss", aw, ah, got.boss) &&
              one(table, "solid", aw, ah, got.solid);
    char name[10] = "digit_0";
    for (uint32_t d = 0; ok && d < 10; ++d) {
        name[6] = static_cast<char>('0' + d);
        ok = one(table, name, aw, ah, got.digit[d]);
    }
    char letter[10] = "letter_a";
    for (uint32_t l = 0; ok && l < 26; ++l) {
        letter[7] = static_cast<char>('a' + l);
        ok = one(table, letter, aw, ah, got.letter[l]);
    }
    if (!ok) return false;

    // Пиксели у вызывающего уже свои (BC7 из бандла или процедурные) — нарезка ложится поверх них
    // целиком, а не по одному полю: половина полей из таблицы и половина из кода была бы третьим
    // источником, худшим из трёх.
    got.px = std::move(atlas.px);
    got.bc7 = std::move(atlas.bc7);
    got.w = table.page_width();
    got.h = table.page_height();
    atlas = std::move(got);
    return true;
}

} // namespace game
