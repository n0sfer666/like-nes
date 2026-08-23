#include "collision.hpp"

#include "../tilemap/query.hpp"

namespace framework::character {
namespace {

// Выигрывает ли ответ сетки у уже найденного. Правила 1–3 из `collision.hpp`, и записаны они здесь
// ОДИН раз: два вызывающих (движение и проба опоры) с двумя копиями этого сравнения — это два
// правила слияния, которые расходятся на первой же правке.
//
// Встречность мерится `dot(normal, travel)`, а не углом: обе нормали сравниваются с ОДНИМ и тем же
// путём, поэтому его длина сокращается в сравнении и нормировать нечего — корня здесь нет.
bool tile_wins(const tilemap::TileHit& tile, const SceneHit& body, Vec2 travel) {
    if (tile.fraction < body.fraction) return true;
    if (body.fraction < tile.fraction) return false;
    // Ничья по встречности отдаёт ответ ТЕЛУ — правило 3.
    return dot(tile.normal, travel) < dot(body.normal, travel);
}

} // namespace

bool cast_nearest(const CollisionScene& s, const CharacterHull& hull, Vec2 position, Vec2 travel,
                  SceneHit& out) {
    bool any = false;
    if (s.world != nullptr) {
        physics::QueryFilter f;
        f.mask = hull.mask;
        physics::RayHit hit;
        if (physics::shapecast(*s.world, hull.shape, position, fix32{}, travel, f, hit)) {
            out = {hit.fraction, hit.normal};
            any = true;
        }
    }
    if (s.grid != nullptr) {
        // Отбор тайлов — по умолчанию, то есть «сплошной». Односторонние тайлы и склоны вертикали 3
        // сделают его вопросом ЗАПРОСА (свип вниз видит односторонний тайл, свип вверх — нет), и
        // тогда фильтр придёт сюда полем сцены; заводить его пустым сейчас значило бы завести
        // настройку без потребителя.
        tilemap::TileFilter f;
        tilemap::TileHit hit;
        if (tilemap::shapecast(*s.grid, hull.shape, position, fix32{}, travel, f, hit)
            && (!any || tile_wins(hit, out, travel))) {
            out = {hit.fraction, hit.normal};
            any = true;
        }
    }
    return any;
}

} // namespace framework::character
