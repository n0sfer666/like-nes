#include "fighter_chain.hpp"

#include <algorithm>

#include "fighter_fail.hpp"
#include "fighter_name.hpp"

namespace framework::brawl {
namespace {

bool cancels(const graphics::ClipSrc& clip) {
    for (const graphics::ClipFrameSrc& f : clip.frames)
        if (f.event == CANCEL_EVENT) return true;
    return false;
}

} // namespace

bool parse_chain(const std::string& value, int line, FighterSpec& f, FighterBakeError& err) {
    f.chain.clear();
    f.chain_line = line;
    std::size_t pos = 0;
    while (pos < value.size()) {
        const std::size_t end = std::min(value.find(' ', pos), value.size());
        if (end > pos) f.chain.push_back(value.substr(pos, end - pos));
        pos = end + 1;
    }
    if (f.chain.empty() || f.chain.size() > MAX_CHAIN)
        return fighter_fail(err, line, "chain must name 1 to " + std::to_string(MAX_CHAIN) + " moves");
    for (const std::string& name : f.chain)
        if (!name_ok(name))
            return fighter_fail(err, line, "a chain move must be a name without '/' or control characters");
    return true;
}

bool chain_moves(const FighterSpec& f, std::vector<uint32_t>& out, FighterBakeError& err) {
    out.clear();
    for (const std::string& name : f.chain) {
        const auto it = std::find_if(f.moves.begin(), f.moves.end(), [&](const MoveSpec& m) { return m.clip == name; });
        if (it == f.moves.end()) return fighter_fail(err, f.chain_line, "chain move '" + name + "' has no move row");
        out.push_back(static_cast<uint32_t>(it - f.moves.begin()));
    }
    return true;
}

bool check_chain_cancels(const FighterSpec& f, std::span<const graphics::ClipSrc> clips, FighterBakeError& err) {
    for (std::size_t i = 0; i + 1 < f.chain.size(); ++i) {
        const std::string name = sheet_clip(f.sheet, f.chain[i]);
        const auto it =
            std::find_if(clips.begin(), clips.end(), [&](const graphics::ClipSrc& c) { return c.name == name; });
        if (it == clips.end() || !cancels(*it))
            return fighter_fail(err, f.chain_line, no_cancel_error(i, name));
    }
    return true;
}

} // namespace framework::brawl
