#include <algorithm>
#include <map>

#include "clip_import.hpp"
#include "clip_parts.hpp"
#include "text_fields.hpp"

namespace framework::graphics {
namespace {

struct Sheet {
    std::string record, file;
    uint32_t line = 0;
    bool has_image = false;
    tiled::Image image;
    uint16_t cell_w = 0, cell_h = 0;
    std::map<std::string, std::size_t> tags;
    std::vector<ClipSrc> clips;
};

using Fields = std::vector<std::string>;

bool fail(const Sheet& s, const std::string& message, std::string& error) {
    error = s.file + ":" + std::to_string(s.line) + ": " + message;
    return false;
}

bool parse_i16(const std::string& text, int16_t& out) {
    const bool negative = !text.empty() && text[0] == '-';
    uint32_t v = 0;
    if (!core::parse_u32(negative ? text.substr(1) : text, v) || v > static_cast<uint32_t>(clip_import::MAX_SIDE))
        return false;
    out = static_cast<int16_t>(negative ? -static_cast<int32_t>(v) : static_cast<int32_t>(v));
    return true;
}

bool image(Sheet& s, const Fields& f, tiled::Source& src, std::string& error) {
    if (f.size() != 2) return fail(s, "expected 'image|<path of the sheet PNG>'", error);
    if (s.has_image) return fail(s, "the image is set twice", error);
    std::string why;
    if (!src.image(s.file, f[1], s.image, why)) return fail(s, why, error);
    s.has_image = true;
    return true;
}

bool grid(Sheet& s, const Fields& f, std::string& error) {
    if (f.size() != 3) return fail(s, "expected 'grid|<cell width>|<cell height>'", error);
    if (s.cell_w != 0) return fail(s, "the grid is set twice", error);
    uint16_t w = 0, h = 0;
    if (!core::parse_u16(f[1], w) || !core::parse_u16(f[2], h) || w == 0 || h == 0 || w > clip_import::MAX_SIDE ||
        h > clip_import::MAX_SIDE)
        return fail(s, "a grid cell is 1 to 32767 whole pixels a side", error);
    s.cell_w = w;
    s.cell_h = h;
    return true;
}

bool cells(Sheet& s, ClipSrc& c, uint32_t row, uint32_t from, uint32_t to, uint32_t ticks, std::string& error) {
    const uint64_t y = uint64_t{row} * s.cell_h;
    const uint32_t last = std::max(from, to);
    if ((uint64_t{last} + 1) * s.cell_w > std::min<uint32_t>(s.image.width, 65535) ||
        y + s.cell_h > std::min<uint32_t>(s.image.height, 65535))
        return fail(s, "clip '" + c.name + "' reaches past the " + std::to_string(s.image.width) + "x" +
                           std::to_string(s.image.height) + " sheet",
                    error);
    if (ticks == 0 || ticks > 65535) return fail(s, "ticks= takes 1 to 65535 ticks a frame", error);
    const int step = from <= to ? 1 : -1;
    for (int64_t col = from;; col += step) {
        ClipFrameSrc frame{static_cast<uint16_t>(col * s.cell_w), static_cast<uint16_t>(y), s.cell_w, s.cell_h,
                           0, 0, static_cast<uint16_t>(ticks), {}, {}};
        clip_import::place(clip_import::Pixel{s.cell_w / 2, s.cell_h}, clip_import::Pixel{}, frame);
        c.frames.push_back(std::move(frame));
        if (col == to) break;
    }
    return true;
}

bool clip(Sheet& s, const Fields& f, std::string& error) {
    if (!s.has_image || s.cell_w == 0) return fail(s, "set 'image' and 'grid' before the first clip", error);
    if (f.size() < 6) return fail(s, "expected 'clip|<name>|row=<r>|from=<a>|to=<b>|ticks=<t>[|loop][|pingpong]'", error);
    if (!clip_import::tag_ok(f[1]) || s.tags.count(f[1]) != 0)
        return fail(s, "clip '" + f[1] + "' needs a name of its own, without '/'", error);
    std::map<std::string, uint32_t> opts;
    bool loop = false, pingpong = false;
    for (std::size_t i = 2; i < f.size(); ++i) {
        const std::size_t eq = f[i].find('=');
        const std::string key = f[i].substr(0, eq);
        uint32_t v = 0;
        bool* flag = f[i] == "loop" ? &loop : f[i] == "pingpong" ? &pingpong : nullptr;
        const bool known = key == "row" || key == "from" || key == "to" || key == "ticks";
        if (flag != nullptr && !*flag)
            *flag = true;
        else if (flag != nullptr || eq == std::string::npos || !known || !core::parse_u32(f[i].substr(eq + 1), v) ||
                 !opts.emplace(key, v).second)
            return fail(s, "clip option '" + f[i] + "' is unknown, repeated or not a whole number", error);
    }
    if (opts.size() != 4) return fail(s, "clip '" + f[1] + "' needs row=, from=, to= and ticks=", error);
    ClipSrc c;
    c.name = s.record + "/" + f[1];
    c.flags = static_cast<uint16_t>((loop ? CLIP_LOOP : CLIP_ONCE) | (pingpong ? CLIP_PINGPONG : 0));
    c.texture_guid = s.image.guid;
    if (!cells(s, c, opts["row"], opts["from"], opts["to"], opts["ticks"], error)) return false;
    s.tags.emplace(f[1], s.clips.size());
    s.clips.push_back(std::move(c));
    return true;
}

ClipFrameSrc* frame_at(Sheet& s, const Fields& f, std::string& error) {
    const auto it = s.tags.find(f[1]);
    uint32_t i = 0;
    if (it == s.tags.end()) {
        fail(s, "clip '" + f[1] + "' is not declared above", error);
        return nullptr;
    }
    std::vector<ClipFrameSrc>& frames = s.clips[it->second].frames;
    if (!core::parse_u32(f[2], i) || i >= frames.size()) {
        fail(s, "clip '" + f[1] + "' has frames 0 to " + std::to_string(frames.size() - 1), error);
        return nullptr;
    }
    return &frames[i];
}

bool box(Sheet& s, const Fields& f, std::string& error) {
    if (f.size() != 8) return fail(s, "expected 'box|<clip>|<frame>|<box>|<x>|<y>|<w>|<h>'", error);
    ClipFrameSrc* frame = frame_at(s, f, error);
    if (frame == nullptr) return false;
    BoxKind kind = BoxKind::Hit;
    uint8_t index = 0;
    if (!clip_import::box_name(f[3], kind, index))
        return fail(s, "box '" + f[3] + "' is unknown; name boxes " + clip_import::BOX_NAMES, error);
    int16_t x = 0, y = 0, w = 0, h = 0;
    if (!parse_i16(f[4], x) || !parse_i16(f[5], y) || !parse_i16(f[6], w) || !parse_i16(f[7], h) || w <= 0 || h <= 0)
        return fail(s, "a box takes whole pixels from the pivot and a positive size", error);
    const int64_t left = s.cell_w / 2;
    if (x < -left || int64_t{x} + w > s.cell_w - left || y < -int64_t{s.cell_h} || int64_t{y} + h > 0)
        return fail(s, "box '" + f[3] + "' leaves its " + std::to_string(s.cell_w) + "x" + std::to_string(s.cell_h) +
                           " cell; the pivot is at the bottom centre",
                    error);
    if (!clip_import::add_box(*frame, kind, index, clip_import::Pixel{}, clip_import::Box{x, y, w, h}))
        return fail(s, "box '" + f[3] + "' is set twice on this frame", error);
    return true;
}

bool event(Sheet& s, const Fields& f, std::string& error) {
    if (f.size() != 4) return fail(s, "expected 'event|<clip>|<frame>|<event>'", error);
    ClipFrameSrc* frame = frame_at(s, f, error);
    if (frame == nullptr) return false;
    if (!clip_import::word_ok(f[3])) return fail(s, "event '" + f[3] + "' must be letters, digits, '_', '-' or '.'", error);
    if (!frame->event.empty()) return fail(s, "this frame already has event '" + frame->event + "'", error);
    frame->event = f[3];
    return true;
}

} // namespace

bool import_sheet(const std::string& record, const std::string& file, std::string_view text, tiled::Source& src,
                  std::vector<ClipSrc>& out, std::string& error) {
    out.clear();
    Sheet s;
    s.record = record;
    s.file = file;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = std::min(text.find('\n', start), text.size());
        std::string body(text.substr(start, nl - start));
        start = nl + 1;
        ++s.line;
        body.resize(std::min(body.find('#'), body.size()));
        if (core::trim(body).empty()) continue;
        const Fields f = core::split_fields(body);
        const bool ok = f[0] == "image" ? image(s, f, src, error)
                        : f[0] == "grid" ? grid(s, f, error)
                        : f[0] == "clip" ? clip(s, f, error)
                        : f[0] == "box"  ? box(s, f, error)
                        : f[0] == "event" ? event(s, f, error)
                                          : fail(s, "unknown line '" + f[0] + "'; expected image, grid, clip, box or event", error);
        if (!ok) return false;
    }
    if (s.clips.empty()) return fail(s, "the sheet declares no clip", error);
    out = std::move(s.clips);
    return true;
}

} // namespace framework::graphics
