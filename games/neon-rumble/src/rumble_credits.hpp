#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bundle_lookup.hpp"
#include "credits_read.hpp"
#include "font_read.hpp"
#include "quad_batch.hpp"
#include "rumble_layers.hpp"
#include "text_layout.hpp"

namespace rumble {

struct Level;

// Титры из `game.bundle` (спека #24, В8б): шрифт `monogram` секции `fonts` с атласом сырым RGBA8 и
// секция `credits`, испечённая из `credits.txt` гейта лицензий. Смотрят в маппинг файла уровня.
struct Credits {
    static constexpr const char* FONT = "monogram";

    framework::graphics::FontTable fonts;
    framework::graphics::FontView font;
    framework::core::CreditTable table;
    asset::RgbaView atlas;
    std::string text;

    bool open(const Level& level);
};

struct CreditStats {
    uint32_t lines = 0;
    uint32_t glyphs = 0;
    uint32_t unknown = 0;
    uint32_t quads = 0;
    uint32_t dropped = 0;
};

// Экран F1 поверх уровня: затемнение показанного кадра квадом сплошной текстуры `solid_texture`,
// заголовок и строки титров внутри ЗОНЫ — квадами атласа `font_texture`, перенос по её ширине;
// глифы ниже зоны не рисуются и идут в dropped.
class CreditsQuads {
public:
    static constexpr uint32_t CAPACITY = 2048;
    static constexpr uint32_t MARGIN = 12;

    CreditsQuads();
    CreditStats add(const Credits& credits, const framework::graphics::ViewportFit& fit, Layers& layers,
                    LayerStats& st, uint32_t font_texture, uint32_t solid_texture);

private:
    std::vector<framework::graphics::GlyphPlace> places_;
    std::vector<render::Quad> quads_;
};

} // namespace rumble
