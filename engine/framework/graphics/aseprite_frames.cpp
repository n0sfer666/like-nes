#include <map>
#include <utility>

#include "aseprite_parts.hpp"

namespace framework::graphics::clip_import {
namespace {

constexpr int64_t MAX_COORD = 65535;

bool in(int64_t v, int64_t lo, int64_t hi) { return v >= lo && v <= hi; }

bool read_frame(const tiled::Doc& d, const json::Node& f, const tiled::Image& sheet, AseFrame& out,
                std::string& error) {
    if (f.kind != json::Kind::Object) return d.fail(f, "a frame must be an object", error);
    Box r, trim, size;
    int64_t ms = 0;
    bool rotated = false;
    if (!read_box(d, f, "frame", false, r, error) || !read_box(d, f, "spriteSourceSize", false, trim, error) ||
        !read_box(d, f, "sourceSize", true, size, error) ||
        !d.get(f, "duration", ms, error, tiled::Need::Required) || !d.get(f, "rotated", rotated, error))
        return false;
    if (rotated) return d.fail(f, "a rotated frame is not supported; export the sheet without rotation", error);
    if (!in(r.x, 0, MAX_COORD) || !in(r.y, 0, MAX_COORD) || !in(r.w, 1, MAX_COORD) || !in(r.h, 1, MAX_COORD) ||
        r.x + r.w > static_cast<int64_t>(sheet.width) || r.y + r.h > static_cast<int64_t>(sheet.height))
        return d.fail(f, "'frame' lies outside the " + std::to_string(sheet.width) + "x" +
                             std::to_string(sheet.height) + " sheet",
                      error);
    if (!in(size.w, 1, MAX_SIDE) || !in(size.h, 1, MAX_SIDE))
        return d.fail(f, "'sourceSize' must be 1 to 32767 pixels a side", error);
    if (trim.w != r.w || trim.h != r.h || !in(trim.x, 0, size.w - r.w) || !in(trim.y, 0, size.h - r.h))
        return d.fail(f, "'spriteSourceSize' must be the size of 'frame' and lie inside 'sourceSize'", error);
    if (!ticks_of_ms(ms, out.ticks))
        return d.fail(f, "'duration' must be 1 to 1000000 ms and stay under 65536 ticks", error);
    out.x = static_cast<uint16_t>(r.x);
    out.y = static_cast<uint16_t>(r.y);
    out.w = static_cast<uint16_t>(r.w);
    out.h = static_cast<uint16_t>(r.h);
    out.trim = Pixel{trim.x, trim.y};
    out.src_w = size.w;
    out.src_h = size.h;
    return true;
}

bool trailing_number(std::string_view name, uint64_t& out) {
    const std::size_t dot = name.rfind('.');
    if (dot != std::string_view::npos && name.find_first_not_of("0123456789", dot + 1) != std::string_view::npos)
        name = name.substr(0, dot);
    std::size_t start = name.size();
    while (start > 0 && name[start - 1] >= '0' && name[start - 1] <= '9') --start;
    const std::size_t digits = name.size() - start;
    if (digits == 0 || digits > 9) return false;
    out = 0;
    for (const char c : name.substr(start)) out = out * 10 + static_cast<uint64_t>(c - '0');
    return true;
}

bool numbering(const tiled::Doc& d, std::span<const std::pair<const json::Node*, std::string_view>> names,
               std::string& error) {
    std::vector<uint64_t> numbers;
    for (const auto& [at, name] : names) {
        uint64_t n = 0;
        if (!trailing_number(name, n)) return true;
        numbers.push_back(n);
    }
    if (numbers.size() > 1 && numbers[0] != 0)
        return d.fail(*names[0].first, "frame '" + std::string(names[0].second) +
                                           "' opens the sheet but is not frame 0; export without --ignore-empty "
                                           "or --frame-range",
                      error);
    for (std::size_t i = 1; i < numbers.size(); ++i) {
        if (numbers[i] == numbers[i - 1] + 1) continue;
        const std::string why = numbers[i] > numbers[i - 1]
                                    ? "skips a frame number; export without --ignore-empty"
                                    : "repeats a frame number; export without --split-layers, --split-tags or "
                                      "--split-slices";
        return d.fail(*names[i].first, "frame '" + std::string(names[i].second) + "' " + why, error);
    }
    return true;
}

} // namespace

bool read_box(const tiled::Doc& d, const json::Node& obj, std::string_view key, bool size_only, Box& out,
              std::string& error) {
    const json::Node* n = nullptr;
    if (!d.expect(obj, key, json::Kind::Object, tiled::Need::Required, n, error)) return false;
    if (!size_only && (!d.get(*n, "x", out.x, error, tiled::Need::Required) ||
                       !d.get(*n, "y", out.y, error, tiled::Need::Required)))
        return false;
    return d.get(*n, "w", out.w, error, tiled::Need::Required) && d.get(*n, "h", out.h, error, tiled::Need::Required);
}

bool read_frames(const tiled::Doc& d, const json::Node& frames, const tiled::Image& sheet,
                 std::vector<AseFrame>& out, std::string& error) {
    const bool hash = frames.kind == json::Kind::Object;
    if (!hash && frames.kind != json::Kind::Array)
        return d.fail(frames, "'frames' must be an array (json-array) or an object (json-hash)", error);
    if (frames.count == 0 || frames.count > 65535) return d.fail(frames, "'frames' must hold 1 to 65535 frames", error);
    const std::span<const json::Node> nodes =
        std::span<const json::Node>(d.nodes).subspan(frames.first, hash ? 2u * frames.count : frames.count);
    std::vector<std::pair<const json::Node*, std::string_view>> names;
    std::map<std::pair<uint16_t, uint16_t>, std::size_t> cells;
    for (uint32_t i = 0; i < frames.count; ++i) {
        const json::Node& f = hash ? nodes[2u * i + 1] : nodes[i];
        std::string_view name = hash ? nodes[2u * i].s : std::string_view();
        if (!hash && f.kind == json::Kind::Object && !d.get(f, "filename", name, error)) return false;
        AseFrame a;
        if (!read_frame(d, f, sheet, a, error)) return false;
        const auto [it, fresh] = cells.emplace(std::make_pair(a.x, a.y), i);
        if (!fresh)
            return d.fail(f, "frames " + std::to_string(it->second) + " and " + std::to_string(i) +
                                 " share one cell of the sheet; export without --merge-duplicates",
                          error);
        names.emplace_back(&f, name);
        out.push_back(a);
    }
    return numbering(d, names, error);
}

} // namespace framework::graphics::clip_import
