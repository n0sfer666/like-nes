#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "bundle_lookup.hpp"
#include "credits_read.hpp"
#include "font_read.hpp"
#include "quad_batch.hpp"
#include "rumble_layers.hpp"
#include "text_layout.hpp"
#include "text_quads.hpp"

namespace rumble {

struct Level;

// Титры из `game.bundle` (спека #24, В8б): шрифт `monogram` секции `fonts` с атласом сырым RGBA8 и
// секция `credits`, испечённая из `credits.txt` гейта лицензий, — по блоку текста на пак. Смотрят в
// маппинг файла уровня.
struct Credits {
    static constexpr const char* FONT = "monogram";

    framework::graphics::FontTable fonts;
    framework::graphics::FontView font;
    framework::core::CreditTable table;
    asset::RgbaView atlas;
    std::vector<std::string> blocks;

    bool open(const Level& level);
};

struct CreditStats {
    uint32_t pages = 0;
    uint32_t lines = 0;
    uint32_t glyphs = 0;
    uint32_t unknown = 0;
    uint32_t quads = 0;
    uint32_t dropped = 0;
};

// Экран F1 поверх уровня: затемнение показанного кадра квадом сплошной текстуры `solid_texture`,
// заголовок с номером страницы и паки страницы `page` внутри ЗОНЫ — квадами атласа `font_texture`,
// перенос по её ширине. Страница — столько целых паков, сколько влезает по высоте зоны; глифы пака,
// который не влез и на пустую страницу, ниже зоны не рисуются и идут в dropped. `pages` в ответе —
// число страниц и при `page` за последней, когда ничего не рисуется.
class CreditsQuads {
public:
    static constexpr uint32_t CAPACITY = 2048;
    static constexpr uint32_t MARGIN = 12;

    CreditsQuads();
    CreditStats add(const Credits& credits, uint32_t page, const framework::graphics::ViewportFit& fit,
                    Layers& layers, LayerStats& st, uint32_t font_texture, uint32_t solid_texture);

private:
    struct Frame {
        uint32_t width = 1;
        uint32_t scale = 1;
        int32_t left = 0;
        int32_t top = 0;
        int32_t bottom = 0;
        int32_t line_px = 0;
    };

    static Frame frame(const Credits& credits, const framework::graphics::ViewportFit& fit);
    void paginate(const Credits& credits, const Frame& f);
    void draw(const Credits& credits, std::string_view text, const Frame& f, framework::graphics::TextPen& pen,
              Layers& layers, LayerStats& st, uint32_t font_texture, CreditStats& out);

    std::vector<framework::graphics::GlyphPlace> places_;
    std::vector<render::Quad> quads_;
    std::vector<uint32_t> first_;
};

} // namespace rumble
