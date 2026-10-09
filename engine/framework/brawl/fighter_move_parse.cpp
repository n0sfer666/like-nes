#include "fighter_move_parse.hpp"

#include <iterator>

#include "fighter_fail.hpp"
#include "fighter_name.hpp"

namespace framework::brawl {
namespace {

const char* const TYPE_NAMES[] = {"light", "heavy", "launch", "grab", "throw"};
static_assert(std::size(TYPE_NAMES) == HIT_TYPE_LAST + 1u);

} // namespace

bool open_move(FighterSpec& f, const std::vector<std::string>& fields, int line, FighterBakeError& err) {
    const std::string& box = fields[2];
    if (box.size() != 4 || box.compare(0, 3, "hit") != 0 || box[3] < '0' || box[3] >= '0' + MAX_HIT_BOXES)
        return fighter_fail(err, line, "move box '" + box + "' must be hit0..hit3");
    if (!name_ok(fields[1]))
        return fighter_fail(err, line, "a move name must be a name without '/' or control characters");
    MoveSpec m;
    m.name = fields[1];
    m.clip = fields[1];
    m.line = line;
    m.strike.box = static_cast<uint8_t>(box[3] - '0');
    for (const MoveSpec& o : f.moves)
        if (o.name == m.name && o.strike.box == m.strike.box)
            return fighter_fail(err, line, "move '" + m.name + "' " + box + " is declared twice");
    f.moves.push_back(m);
    return true;
}

bool set_move_text(MoveSpec& m, const std::string& key, const std::string& value, int line, FighterBakeError& err) {
    if (key == "clip") {
        if (!name_ok(value)) return fighter_fail(err, line, "a move clip must be a name without '/' or control characters");
        m.clip = value;
        return true;
    }
    if (key == "hits_down" || key == "slide") {
        if (value != "yes" && value != "no") return fighter_fail(err, line, key + " must be yes or no");
        (key == "slide" ? m.strike.slides : m.strike.hits_down) = value == "yes";
        return true;
    }
    if (key != "type") return fighter_fail(err, line, "unknown key '" + key + "' in a move");
    for (uint8_t t = 0; t <= HIT_TYPE_LAST; ++t)
        if (value == TYPE_NAMES[t]) {
            m.strike.type = static_cast<HitType>(t);
            return true;
        }
    return fighter_fail(err, line, "type must be light, heavy, launch, grab or throw");
}

bool same_rows(const FighterSpec& f, FighterBakeError& err) {
    for (std::size_t i = 0; i < f.moves.size(); ++i)
        for (std::size_t j = 0; j < i; ++j) {
            const MoveSpec& a = f.moves[j];
            const MoveSpec& b = f.moves[i];
            if (a.name != b.name) continue;
            if (a.clip != b.clip) return fighter_fail(err, b.line, "move '" + b.name + "' plays two clips");
            if (a.strike.slides != b.strike.slides)
                return fighter_fail(err, b.line, "move '" + b.name + "' rows disagree on slide");
        }
    return true;
}

} // namespace framework::brawl
