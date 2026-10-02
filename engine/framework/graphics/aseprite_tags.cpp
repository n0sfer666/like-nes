#include <algorithm>

#include "aseprite_parts.hpp"
#include "text_fields.hpp"

namespace framework::graphics::clip_import {
namespace {

bool direction(const tiled::Doc& d, const json::Node& t, AseTag& out, std::string& error) {
    std::string_view dir = "forward";
    if (!d.get(t, "direction", dir, error)) return false;
    const bool pingpong = dir == "pingpong" || dir == "pingpong_reverse";
    if (!pingpong && dir != "forward" && dir != "reverse")
        return d.fail(t, "tag '" + out.name + "' has direction '" + std::string(dir) +
                             "'; expected forward, reverse, pingpong or pingpong_reverse",
                      error);
    out.reverse = dir == "reverse" || dir == "pingpong_reverse";
    const json::Node* repeat = d.field(t, "repeat");
    bool once = false;
    if (repeat != nullptr) {
        once = (repeat->kind == json::Kind::String && repeat->s == "1") ||
               (repeat->kind == json::Kind::Int && repeat->i == 1);
        if (!once)
            return d.fail(*repeat, "tag '" + out.name +
                                       "' repeats a set number of times; the engine plays a clip once "
                                       "(repeat 1, a ping-pong tag makes one pass) or loops (no repeat)",
                          error);
    }
    out.flags = static_cast<uint16_t>(once ? CLIP_ONCE : (CLIP_LOOP | (pingpong ? CLIP_PINGPONG : 0)));
    return true;
}

bool events(const tiled::Doc& d, const json::Node& t, AseTag& out, std::string& error) {
    out.events.assign(out.to - out.from + 1, std::string());
    std::string_view data;
    if (!d.get(t, "data", data, error)) return false;
    if (data.empty()) return true;
    const json::Node& at = *d.field(t, "data");
    const std::string grammar = "tag '" + out.name + "' user data must list <frame>:<event>, such as 3:step,7:swing";
    std::size_t start = 0;
    while (start <= data.size()) {
        const std::size_t comma = std::min(data.find(',', start), data.size());
        const std::string item = core::trim(std::string(data.substr(start, comma - start)));
        start = comma + 1;
        const std::size_t colon = item.find(':');
        if (colon == std::string::npos) return d.fail(at, grammar, error);
        const std::string name = core::trim(item.substr(colon + 1));
        uint32_t frame = 0;
        if (!core::parse_u32(core::trim(item.substr(0, colon)), frame) || !word_ok(name))
            return d.fail(at, grammar, error);
        if (frame >= out.events.size())
            return d.fail(at, "event '" + item + "' lies outside tag '" + out.name + "' of " +
                                 std::to_string(out.events.size()) + " frames",
                          error);
        if (!out.events[frame].empty())
            return d.fail(at, "tag '" + out.name + "' has two events on frame " + std::to_string(frame), error);
        out.events[frame] = name;
    }
    return true;
}

bool read_tag(const tiled::Doc& d, const json::Node& t, std::size_t frames, AseTag& out, std::string& error) {
    if (t.kind != json::Kind::Object) return d.fail(t, "a tag must be an object", error);
    std::string_view name;
    int64_t from = 0, to = 0;
    if (!d.get(t, "name", name, error, tiled::Need::Required) || !d.get(t, "from", from, error, tiled::Need::Required) ||
        !d.get(t, "to", to, error, tiled::Need::Required))
        return false;
    out.name = std::string(name);
    if (!tag_ok(name)) return d.fail(t, "tag name '" + out.name + "' must be non-empty and hold no '/'", error);
    if (from < 0 || from > to || to >= static_cast<int64_t>(frames))
        return d.fail(t, "tag '" + out.name + "' spans frames " + std::to_string(from) + " to " + std::to_string(to) +
                             " of " + std::to_string(frames),
                      error);
    out.from = static_cast<uint32_t>(from);
    out.to = static_cast<uint32_t>(to);
    return direction(d, t, out, error) && events(d, t, out, error);
}

} // namespace

bool read_tags(const tiled::Doc& d, const json::Node& meta, std::size_t frames, std::vector<AseTag>& out,
               std::string& error) {
    const json::Node* list = nullptr;
    if (!d.expect(meta, "frameTags", json::Kind::Array, tiled::Need::Optional, list, error)) return false;
    if (list == nullptr || list->count == 0)
        return d.fail(meta, "no tags in 'frameTags'; a clip is a tag, export with --list-tags", error);
    for (const json::Node& t : d.items(*list)) {
        AseTag tag;
        if (!read_tag(d, t, frames, tag, error)) return false;
        for (const AseTag& seen : out)
            if (seen.name == tag.name)
                return d.fail(t, "tag '" + tag.name + "' is declared twice; a clip takes its name from its tag", error);
        out.push_back(std::move(tag));
    }
    return true;
}

} // namespace framework::graphics::clip_import
