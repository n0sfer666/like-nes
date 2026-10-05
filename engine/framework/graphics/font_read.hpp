#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "font_format.hpp"
#include "section_open.hpp"

namespace framework::graphics {

struct FontView {
    const FontRow* row = nullptr;
    std::span<const FontGlyph> glyphs;
};

class FontTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return view_.header != nullptr; }
    uint32_t count() const { return valid() ? view_.header->count : 0; }
    const char* name(uint32_t index) const;
    FontView font(uint32_t index) const;
    FontView find(const char* name) const;

private:
    core::SectionView view_;
    const FontRow* rows_ = nullptr;
};

bool font_glyph_ok(const FontGlyph& g, uint16_t line_height, uint16_t page_w, uint16_t page_h);
const FontGlyph* font_glyph(const FontView& font, uint32_t codepoint);

} // namespace framework::graphics
