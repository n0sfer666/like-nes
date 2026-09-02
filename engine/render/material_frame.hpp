#pragma once

#include <cstdint>
#include <vector>

#include "material_scene.hpp"

struct GpuContext;

namespace matgold {

// Кадр сцены материалов offscreen: свой проход и своё чтение обратно, потому что `capture::
// render_offscreen` рисует `Renderer` (deferred-тракт спеки #2), а здесь тракт другой — один
// проход и пайплайны из кэша материалов. Число вызовов отрисовки возвращается наружу: оно и есть
// предмет гейта 5, а по картинке его не видно.
std::vector<uint8_t> render_frame(GpuContext& gpu, Scene& scene, uint32_t w, uint32_t h,
                                  uint32_t& draws);

} // namespace matgold
