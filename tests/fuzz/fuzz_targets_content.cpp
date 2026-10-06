#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "credits_bake.hpp"
#include "credits_read.hpp"
#include "fighter_bake.hpp"
#include "fighter_read.hpp"
#include "font_bake.hpp"
#include "font_read.hpp"
#include "framework_brawl_fighter_fixture.hpp"
#include "fuzz_target.hpp"
#include "text_layout.hpp"

namespace fuzz {
namespace {

namespace gr = framework::graphics;
namespace co = framework::core;
namespace br = framework::brawl;

std::vector<uint8_t> seed_fonts() {
    gr::FontSrc f;
    f.name = "mono";
    f.line_height = 12;
    f.texture_guid = 0xF1;
    f.page_w = 21;
    f.page_h = 12;
    f.glyphs = {{' ', 0, 0, 0, 6}, {'?', 0, 0, 5, 6}, {'A', 7, 0, 5, 6}, {0x416, 14, 0, 7, 8}};
    std::vector<uint8_t> bytes;
    std::string err;
    gr::bake_fonts({&f, 1}, bytes, err);
    return bytes;
}

bool read_fonts(const uint8_t* data, size_t size) {
    gr::FontTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const char* fname = t.name(i);
        consume_str(fname);
        const gr::FontView f = t.font(i);
        consume(t.find(fname).row != nullptr);
        if (f.row == nullptr) continue;
        consume_all(f.row->line_height, f.row->texture_guid, f.row->page_w, f.row->page_h);
        for (const gr::FontGlyph& g : f.glyphs) {
            consume_all(g.codepoint, g.x, g.y, g.w, g.advance);
            consume(gr::font_glyph(f, g.codepoint) == &g);
        }
        std::array<gr::GlyphPlace, 16> places{};
        const gr::TextStats s = gr::layout_text(f, "A?\xD0\x96 Z\n\xFF" "AAAAAA", 24, places);
        consume_all(s.placed, s.dropped, s.unknown, s.lines, s.width);
        for (uint32_t k = 0; k < s.placed; ++k) consume_all(places[k].x, places[k].y, places[k].glyph->w);
    }
    return true;
}

std::vector<uint8_t> seed_credits() {
    const co::CreditSrc c[] = {{"Warped City", "Ansimuz", "CC0-1.0", "https://ansimuz.itch.io/warped-city", ""},
                               {"monogram", "datagoblin", "CC0-1.0", "https://datagoblin.itch.io/monogram", "Font"}};
    std::vector<uint8_t> bytes;
    std::string err;
    co::bake_credits(c, bytes, err);
    return bytes;
}

bool read_credits(const uint8_t* data, size_t size) {
    co::CreditTable t;
    if (!t.open(data, size)) return false;
    for (uint32_t i = 0; i < t.count(); ++i) {
        const co::Credit c = t.at(i);
        for (const char* s : {c.pack, c.author, c.license, c.url, c.attribution}) consume_str(s);
    }
    return true;
}

std::vector<uint8_t> seed_fighter() {
    std::vector<uint8_t> bytes;
    br::FighterBakeError err;
    br::bake_fighter("banderas.fighter", br::test::FIGHTER_TEXT, br::test::fixture_clips(), bytes, err);
    return bytes;
}

bool read_fighter(const uint8_t* data, size_t size) {
    br::FighterTable t;
    if (!t.open(data, size)) return false;
    consume_str(t.name());
    consume_str(t.sheet());
    const br::DepthProfile p = t.profile();
    consume_all(p.speed_x.raw, p.speed_z.raw, p.gravity.raw, p.jump_vy.raw, t.run_x().raw, t.depth().raw, t.hp());
    for (uint32_t i = 0; i < t.move_count(); ++i) {
        consume_str(t.move_clip(i));
        br::Strike s;
        consume(t.move(i, s));
        consume_all(s.box, static_cast<uint8_t>(s.type), s.hits_down, s.damage, s.depth.raw, s.hitstop, s.hitstun,
                    s.knock_x.raw, s.knock_y.raw);
    }
    return true;
}

const Target TARGETS[] = {
    {"fonts", seed_fonts, read_fonts},
    {"credits", seed_credits, read_credits},
    {"fighter", seed_fighter, read_fighter},
};

} // namespace

const Target* content_targets(std::size_t* count) {
    *count = sizeof(TARGETS) / sizeof(TARGETS[0]);
    return TARGETS;
}

} // namespace fuzz
