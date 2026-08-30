#include "nine_slice.hpp"

namespace framework::graphics {
namespace {

const fix32 HALF = fix32::from_raw(fix32::ONE / 2);

fix32 clamp_corner(fix32 c, fix32 limit) {
    if (c.raw < 0) return fix32{};
    return c.raw > limit.raw ? limit : c;
}

} // namespace

void nine_slice(SpriteList& out, const NineSliceRegions& regions, Vec2 center, Vec2 half,
                Vec2 corner, uint32_t rgba, uint16_t material, int16_t layer) {
    const fix32 cw = clamp_corner(corner.x, half.x);
    const fix32 ch = clamp_corner(corner.y, half.y);
    // Полуразмеры трёх колонок и трёх строк. Середина вырождается законно — у панели шириной ровно
    // в два угла её нет вовсе, — и вырождение поэтому ПРОПУСКАЕТСЯ, а не считается невыданным
    // спрайтом: рисовать там нечего, а не «не поместилось» (то же различение, что у рамки шага E).
    const fix32 hx[3] = {cw * HALF, half.x - cw, cw * HALF};
    const fix32 hy[3] = {ch * HALF, half.y - ch, ch * HALF};
    const fix32 x[3] = {center.x - half.x + hx[0], center.x, center.x + half.x - hx[2]};
    const fix32 y[3] = {center.y - half.y + hy[0], center.y, center.y + half.y - hy[2]};
    const RegionId grid[3][3] = {
        {regions.tl, regions.t, regions.tr},
        {regions.l, regions.c, regions.r},
        {regions.bl, regions.b, regions.br},
    };
    // Порядок обхода — строки сверху вниз, и он наблюдаем: спрайты одного слоя и материала
    // различаются только номером подачи, то есть именно этот обход и задаёт их порядок в батче.
    for (int row = 0; row < 3; ++row) {
        if (hy[row].raw <= 0) continue;
        for (int col = 0; col < 3; ++col) {
            if (hx[col].raw <= 0) continue;
            Sprite s;
            s.center = {x[col], y[row]};
            s.half = {hx[col], hy[row]};
            s.rgba = rgba;
            s.region = grid[row][col];
            s.material = material;
            s.layer = layer;
            out.push(s);
        }
    }
}

} // namespace framework::graphics
