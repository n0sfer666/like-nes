#include "shadow_checks.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "../light/table.hpp"
#include "gpu.hpp"
#include "light_checks.hpp"
#include "light_frame.hpp"

namespace lightgold {
namespace {

// Один источник, светящий сбоку: тень от него ложится ПОПЕРЁК сцены, а не под спрайт. Два набора
// отличаются ровно одной строкой — мягкостью.
const char* const HARD_SHADOW =
    "ambient | 0.10, 0.12, 0.18 | 0.35\n"
    "light | side | point\n"
    "set | pos | -0.80, 0.10\n"
    "set | height | 0.10\n"
    "set | color | 1.0, 0.9, 0.8\n"
    "set | intensity | 3.0\n"
    "set | radius | 2.0\n"
    "set | shadow | 0.00\n";
const char* const SOFT_SHADOW =
    "ambient | 0.10, 0.12, 0.18 | 0.35\n"
    "light | side | point\n"
    "set | pos | -0.80, 0.10\n"
    "set | height | 0.10\n"
    "set | color | 1.0, 0.9, 0.8\n"
    "set | intensity | 3.0\n"
    "set | radius | 2.0\n"
    "set | shadow | 0.12\n";

} // namespace

void shadows_come_from_slots(GpuContext& gpu, matgold::Scene& scene, const mat::Table& mtable,
                             lightgfx::Pass& pass, slotgfx::Pass& normals,
                             slotgfx::Pass& occluders, uint32_t w, uint32_t h) {
    std::printf("  occluders: %u mapped from the table, %u open, %u naming an unknown asset\n",
                occluders.mapped(), occluders.flat(), occluders.missing());
    check(occluders.missing() == 0, "every occlusion slot of the library names an asset that exists");
    check(occluders.mapped() != 0 && occluders.flat() != 0,
          "the library has both materials that cast and materials that do not");
    // Два слота разошлись ЧИСЛОМ: одинаковые наборы прошли бы и у прохода, читающего чужое имя.
    check(occluders.mapped() != normals.mapped(),
          "the occlusion slot names a different set of materials than the normal slot");
    for (uint32_t m = 0; m < mtable.count(); ++m) {
        const bool from_table = mtable.texture_of(m, "occlusion") >= 0;
        if ((occluders.asset(m) != nullptr) == from_table) continue;
        std::printf("  FAIL: material %s: slot says %s, the pass bound %s\n",
                    mtable.name(mtable.row(m).name_off),
                    from_table ? "an occluder" : "nothing",
                    occluders.asset(m) ? occluders.asset(m) : "the open map");
        check(false, "the pass follows the table for this material");
    }

    uint32_t d = 0;
    const std::vector<uint8_t> open =
        lightgold::render_frame(gpu, scene, Graph{&pass, &normals}, w, h, d);
    const std::vector<uint8_t> shadowed =
        lightgold::render_frame(gpu, scene, Graph{&pass, &normals, &occluders}, w, h, d);
    check(!shadowed.empty() && shadowed != open, "occluders change the lit frame");
    if (shadowed.size() != open.size()) {
        check(false, "both frames have the same size");
        return;
    }
    // Тень только ГАСИТ. Утверждение ловит перевёрнутый марш: набравший `1 - acc` вместо `acc`
    // тоже меняет кадр, и «кадр изменился» зелено на нём точно так же. Строгое неравенство хоть
    // где-то — второй половиной: кадр, совпавший с исходным во всех пикселях, тоже «не светлее».
    bool darker_somewhere = false;
    bool brighter_anywhere = false;
    for (size_t i = 0; i < shadowed.size(); ++i) {
        if (i % 4 == 3) continue;   // альфа перекрытием не трогается
        if (shadowed[i] > open[i] + 1) brighter_anywhere = true;
        if (shadowed[i] + 1 < open[i]) darker_somewhere = true;
    }
    check(!brighter_anywhere, "a shadow only darkens: no pixel gets brighter than the open frame");
    check(darker_somewhere, "a shadow darkens somewhere: the occluders are not a no-op");
}

void softness_comes_from_the_table(GpuContext& gpu, matgold::Scene& scene,
                                   slotgfx::Pass& normals, slotgfx::Pass& occluders, uint32_t w,
                                   uint32_t h) {
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    std::vector<uint8_t> bh, bs;
    light::Table th, ts;
    if (!load_table(HARD_SHADOW, bh, th, "the hard-shadow fixture")) return;
    if (!load_table(SOFT_SHADOW, bs, ts, "the soft-shadow fixture")) return;

    lightgfx::Pass hard, soft;
    check(hard.init(gpu.device, gpu.queue, th, WGPUTextureFormat_RGBA8Unorm, aspect),
          "a pass over the hard-edged source starts");
    check(soft.init(gpu.device, gpu.queue, ts, WGPUTextureFormat_RGBA8Unorm, aspect),
          "a pass over the soft-edged source starts");
    uint32_t d = 0;
    const std::vector<uint8_t> fh =
        lightgold::render_frame(gpu, scene, Graph{&hard, &normals, &occluders}, w, h, d);
    const std::vector<uint8_t> fs =
        lightgold::render_frame(gpu, scene, Graph{&soft, &normals, &occluders}, w, h, d);
    check(!fh.empty() && fh != fs, "the shadow softness of the table changes the frame");
    soft.shutdown();
    hard.shutdown();
}

} // namespace lightgold
