#include "rumble_banner.hpp"

#include "text_quads.hpp"

namespace rumble {

namespace {

using namespace framework::graphics;

constexpr uint32_t MARGIN = 12;
constexpr uint32_t PAD = 4;
constexpr uint32_t BACK_RGBA = 0x000000A0u;
constexpr uint32_t TEXT_RGBA = 0xFFE45CFFu;

std::string player(int p) { return "P" + std::to_string(p + 1); }

} // namespace

std::string banner_text(const framework::input::Lobby& lobby) {
    for (int p = 0; p < lobby.seats(); ++p)
        if (lobby.lost(p)) return player(p) + ": reconnect the pad";
    for (int p = 0; p < lobby.seats(); ++p)
        if (lobby.state(p) == framework::input::SeatState::Free) return player(p) + ": press a button to join";
    return {};
}

BannerQuads::BannerQuads() : places_(CAPACITY), quads_(CAPACITY) {}

void BannerQuads::add(const Credits& credits, std::string_view text, const ViewportFit& fit, Layers& layers,
                      LayerStats& st, uint32_t font_texture, uint32_t solid_texture) {
    if (text.empty() || credits.font.row == nullptr) return;
    const PixelRect& z = fit.zone;
    const uint32_t scale = fit.scale == 0 ? 1 : fit.scale;
    const TextStats t = layout_text(credits.font, text, z.w / scale, places_);
    const auto w = static_cast<int32_t>(t.width * scale);
    const auto h = static_cast<int32_t>(credits.font.row->line_height * scale);
    const auto pad = static_cast<int32_t>(PAD * scale);
    const int32_t x = z.x + (static_cast<int32_t>(z.w) - w) / 2;
    const int32_t y = z.y + static_cast<int32_t>(z.h) - static_cast<int32_t>(MARGIN * scale) - h;
    render::Quad back;
    back.x = static_cast<float>(x - pad);
    back.y = static_cast<float>(y - pad);
    back.w = static_cast<float>(w + 2 * pad);
    back.h = static_cast<float>(h + 2 * pad);
    back.tw = 1;
    back.th = 1;
    back.rgba = BACK_RGBA;
    layers.append(st, {&back, 1}, solid_texture);
    const TextQuadStats q = text_quads(credits.font, {places_.data(), t.placed}, TextPen{x, y, scale, TEXT_RGBA}, quads_);
    layers.append(st, {quads_.data(), q.quads}, font_texture);
}

} // namespace rumble
