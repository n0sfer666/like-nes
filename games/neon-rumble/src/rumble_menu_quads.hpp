#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

#include "quad_batch.hpp"
#include "rumble_credits.hpp"
#include "rumble_layers.hpp"
#include "rumble_menu.hpp"
#include "text_layout.hpp"

namespace rumble {

// Текстуры меню: уровня и бойцов нет, в GPU только сплошная и атлас шрифта титров (`init_menu_quads`).
constexpr uint32_t MENU_SOLID_TEXTURE = 0;
constexpr uint32_t MENU_FONT_TEXTURE = 1;

struct MenuStats {
    uint32_t unknown = 0;
    uint32_t dropped = 0;
};

// Меню шрифтом титров по центру ЗОНЫ: строка ошибки над пунктами, выбранный пункт — с «> » и цветом.
class MenuQuads {
public:
    static constexpr uint32_t CAPACITY = 256;

    MenuQuads();
    MenuStats add(const Menu& menu, const Credits& credits, const framework::graphics::ViewportFit& fit, Layers& layers,
             LayerStats& st);

private:
    int32_t line(const Credits& credits, std::string_view text, uint32_t rgba,
                 const framework::graphics::ViewportFit& fit, int32_t y, Layers& layers, LayerStats& st,
                 MenuStats& ms);

    std::vector<framework::graphics::GlyphPlace> places_;
    std::vector<render::Quad> quads_;
};

} // namespace rumble
