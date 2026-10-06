#include "rumble_credits.hpp"

#include <cstdio>

#include "rumble_level.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

constexpr const char* TITLE = "Credits \xC2\xB7 \xD0\xA2\xD0\xB8\xD1\x82\xD1\x80\xD1\x8B";
constexpr const char* DASH = " \xE2\x80\x94 ";
constexpr uint32_t DIM_RGBA = 0x000000C0u;
constexpr uint32_t TITLE_RGBA = 0x5CF2FFFFu;
constexpr uint32_t BODY_RGBA = 0xFFFFFFFFu;

std::vector<std::string> compose(const framework::core::CreditTable& table) {
    std::vector<std::string> out;
    for (uint32_t i = 0; i < table.count(); ++i) {
        const framework::core::Credit c = table.at(i);
        std::string block = std::string(c.pack) + DASH + c.author + ", " + c.license + "\n" + c.url;
        if (*c.attribution != '\0') block += std::string("\n") + c.attribution;
        out.push_back(std::move(block));
    }
    return out;
}

void title_text(char (&out)[64], uint32_t page, uint32_t pages) {
    std::snprintf(out, sizeof out, "%s  %u/%u", TITLE, page, pages);
}

} // namespace

// docs:begin(credits-open)
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
    blocks = compose(table);
    return true;
}
// docs:end(credits-open)

CreditsQuads::CreditsQuads() : places_(CAPACITY), quads_(CAPACITY) {}

CreditsQuads::Frame CreditsQuads::frame(const Credits& credits, const ViewportFit& fit) {
    const PixelRect& r = fit.zone;
    Frame f;
    f.scale = fit.scale == 0 ? 1 : fit.scale;
    f.width = r.w / f.scale > 2 * MARGIN ? r.w / f.scale - 2 * MARGIN : 1;
    const auto margin = static_cast<int32_t>(MARGIN * f.scale);
    f.left = r.x + margin;
    f.top = r.y + margin;
    f.bottom = r.y + static_cast<int32_t>(r.h) - margin;
    f.line_px = static_cast<int32_t>(credits.font.row->line_height * f.scale);
    return f;
}

// docs:begin(credits-pages)
void CreditsQuads::paginate(const Credits& credits, const Frame& f) {
    first_.assign(1, 0);
    const auto most = static_cast<uint32_t>(credits.blocks.size() > 0 ? credits.blocks.size() : 1);
    char text[64];
    title_text(text, most, most);
    const int32_t title = static_cast<int32_t>(layout_text(credits.font, text, f.width, places_).lines) + 1;
    const int32_t rows = f.line_px > 0 ? (f.bottom - f.top) / f.line_px - title : 0;
    int32_t used = 0;
    for (uint32_t i = 0; i < credits.blocks.size(); ++i) {
        const auto lines = static_cast<int32_t>(layout_text(credits.font, credits.blocks[i], f.width, places_).lines);
        if (used > 0 && used + lines > rows) {
            first_.push_back(i);
            used = 0;
        }
        used += lines + 1;
    }
}
// docs:end(credits-pages)

// docs:begin(credits-layout)
void CreditsQuads::draw(const Credits& credits, std::string_view text, const Frame& f, TextPen& pen,
                        Layers& layers, LayerStats& st, uint32_t font_texture, CreditStats& out) {
    const TextStats t = layout_text(credits.font, text, f.width, places_);
    uint32_t kept = 0;
    for (uint32_t i = 0; i < t.placed; ++i)
        if (pen.y + places_[i].y * static_cast<int32_t>(f.scale) + f.line_px <= f.bottom) places_[kept++] = places_[i];
    const TextQuadStats q = text_quads(credits.font, {places_.data(), kept}, pen, quads_);
    layers.append(st, {quads_.data(), q.quads}, font_texture);
    out.lines += t.lines;
    out.glyphs += t.placed;
    out.unknown += t.unknown;
    out.quads += q.quads;
    out.dropped += t.dropped + (t.placed - kept) + q.dropped;
    pen.y += static_cast<int32_t>(t.lines + 1) * f.line_px;
    pen.rgba = BODY_RGBA;
}
// docs:end(credits-layout)

CreditStats CreditsQuads::add(const Credits& credits, uint32_t page, const ViewportFit& fit, Layers& layers,
                              LayerStats& st, uint32_t font_texture, uint32_t solid_texture) {
    CreditStats out;
    if (credits.font.row == nullptr) return out;
    const Frame f = frame(credits, fit);
    paginate(credits, f);
    out.pages = static_cast<uint32_t>(first_.size());
    if (page >= out.pages) return out;
    render::Quad dim;
    dim.x = static_cast<float>(fit.shown.x);
    dim.y = static_cast<float>(fit.shown.y);
    dim.w = static_cast<float>(fit.shown.w);
    dim.h = static_cast<float>(fit.shown.h);
    dim.tw = 1;
    dim.th = 1;
    dim.rgba = DIM_RGBA;
    layers.append(st, {&dim, 1}, solid_texture);
    char title[64];
    title_text(title, page + 1, out.pages);
    TextPen pen{f.left, f.top, f.scale, TITLE_RGBA};
    draw(credits, title, f, pen, layers, st, font_texture, out);
    const uint32_t end = page + 1 < out.pages ? first_[page + 1] : static_cast<uint32_t>(credits.blocks.size());
    for (uint32_t i = first_[page]; i < end; ++i) draw(credits, credits.blocks[i], f, pen, layers, st, font_texture, out);
    return out;
}

} // namespace rumble
