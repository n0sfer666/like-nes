#include "rumble_credits.hpp"

#include <cstdio>

#include "rumble_level.hpp"
#include "text_quads.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

constexpr const char* TITLE = "Credits \xC2\xB7 \xD0\xA2\xD0\xB8\xD1\x82\xD1\x80\xD1\x8B";
constexpr const char* DASH = " \xE2\x80\x94 ";
constexpr uint32_t DIM_RGBA = 0x000000C0u;
constexpr uint32_t TITLE_RGBA = 0x5CF2FFFFu;
constexpr uint32_t BODY_RGBA = 0xFFFFFFFFu;

std::string compose(const framework::core::CreditTable& table) {
    std::string out;
    for (uint32_t i = 0; i < table.count(); ++i) {
        const framework::core::Credit c = table.at(i);
        out += std::string(c.pack) + DASH + c.author + ", " + c.license + "\n" + c.url + "\n";
        if (*c.attribution != '\0') out += std::string(c.attribution) + "\n";
        out += "\n";
    }
    return out;
}

} // namespace

bool Credits::open(const Level& level) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!level.read_table("fonts", data, size) || !fonts.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: fonts table: does not open\n");
        return false;
    }
    font = fonts.find(Credits::FONT);
    if (font.row == nullptr) {
        std::fprintf(stderr, "neon-rumble: no font %s in the fonts table\n", Credits::FONT);
        return false;
    }
    const asset::LookupFault f = asset::raw_rgba8(level.bundle, font.row->texture_guid, atlas);
    if (f != asset::LookupFault::Ok || atlas.width != font.row->page_w || atlas.height != font.row->page_h) {
        std::fprintf(stderr, "neon-rumble: font atlas %016llx: %s\n",
                     static_cast<unsigned long long>(font.row->texture_guid),
                     f != asset::LookupFault::Ok ? asset::lookup_fault_name(f) : "size differs from the font page");
        return false;
    }
    if (!level.read_table("credits", data, size) || !table.open(data, size)) {
        std::fprintf(stderr, "neon-rumble: credits table: does not open\n");
        return false;
    }
    text = compose(table);
    return true;
}

CreditsQuads::CreditsQuads() : places_(CAPACITY), quads_(CAPACITY) {}

CreditStats CreditsQuads::add(const Credits& credits, const ViewportFit& fit, Layers& layers, LayerStats& st,
                              uint32_t font_texture, uint32_t solid_texture) {
    CreditStats out;
    if (credits.font.row == nullptr) return out;
    render::Quad dim;
    dim.x = static_cast<float>(fit.shown.x);
    dim.y = static_cast<float>(fit.shown.y);
    dim.w = static_cast<float>(fit.shown.w);
    dim.h = static_cast<float>(fit.shown.h);
    dim.tw = 1;
    dim.th = 1;
    dim.rgba = DIM_RGBA;
    layers.append(st, {&dim, 1}, solid_texture);
    const PixelRect& r = fit.zone;
    const uint32_t scale = fit.scale == 0 ? 1 : fit.scale;
    const uint32_t width = r.w / scale > 2 * MARGIN ? r.w / scale - 2 * MARGIN : 1;
    const auto margin = static_cast<int32_t>(MARGIN * scale);
    const auto line_px = static_cast<int32_t>(credits.font.row->line_height * scale);
    const int32_t bottom = r.y + static_cast<int32_t>(r.h) - margin;
    TextPen pen{r.x + margin, r.y + margin, scale, TITLE_RGBA};
    for (const char* text : {TITLE, credits.text.c_str()}) {
        const TextStats t = layout_text(credits.font, text, width, places_);
        uint32_t kept = 0;
        for (uint32_t i = 0; i < t.placed; ++i)
            if (pen.y + places_[i].y * static_cast<int32_t>(scale) + line_px <= bottom) places_[kept++] = places_[i];
        const TextQuadStats q = text_quads(credits.font, {places_.data(), kept}, pen, quads_);
        layers.append(st, {quads_.data(), q.quads}, font_texture);
        out.lines += t.lines;
        out.glyphs += t.placed;
        out.unknown += t.unknown;
        out.quads += q.quads;
        out.dropped += t.dropped + (t.placed - kept) + q.dropped;
        pen.y += static_cast<int32_t>(t.lines + 1) * line_px;
        pen.rgba = BODY_RGBA;
    }
    return out;
}

} // namespace rumble
