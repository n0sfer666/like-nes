#include "sprite_flip.hpp"

#include <utility>

#include "graphics_sprite.hpp"

namespace framework::graphics {

Texel flip_source(uint8_t flip, uint32_t n, Texel out) {
    Texel t = out;
    if ((flip & SPRITE_FLIP_V) != 0) t.y = n - 1 - t.y;
    if ((flip & SPRITE_FLIP_H) != 0) t.x = n - 1 - t.x;
    if ((flip & SPRITE_FLIP_D) != 0) std::swap(t.x, t.y);
    return t;
}

} // namespace framework::graphics
