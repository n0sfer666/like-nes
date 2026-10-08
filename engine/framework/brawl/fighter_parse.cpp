#include <iterator>

#include "fighter_bake.hpp"
#include "fighter_chain.hpp"
#include "fighter_fail.hpp"
#include "fighter_name.hpp"
#include "text_fields.hpp"

namespace framework::brawl {
namespace {

template <class T>
struct FixKey {
    const char* name;
    fix32 T::* field;
    fix32 lo, hi;
};

template <class T>
struct TickKey {
    const char* name;
    uint32_t T::* field;
    uint32_t lo, hi;
};

const fix32 NO_SPEED = fix32::from_int(0);
const fix32 KNOCK_LO = -MAX_FIGHTER_SPEED;
const fix32 DEPTH_LO = fix32::from_raw(1);

const FixKey<FighterSpec> HEAD_FIX[] = {
    {"speed_x", &FighterSpec::speed_x, NO_SPEED, MAX_FIGHTER_SPEED},
    {"speed_z", &FighterSpec::speed_z, NO_SPEED, MAX_FIGHTER_SPEED},
    {"run_x", &FighterSpec::run_x, NO_SPEED, MAX_FIGHTER_SPEED},
    {"gravity", &FighterSpec::gravity, DEPTH_LO, MAX_FIGHTER_SPEED},
    {"jump_vy", &FighterSpec::jump_vy, NO_SPEED, MAX_FIGHTER_SPEED},
    {"depth", &FighterSpec::depth, DEPTH_LO, MAX_FIGHTER_DEPTH},
};
const TickKey<FighterSpec> HEAD_TICK[] = {
    {"hp", &FighterSpec::hp, MIN_HP, MAX_HP}, {"down", &FighterSpec::down, 1, MAX_HIT_TICKS},
    {"getup", &FighterSpec::getup, 1, MAX_HIT_TICKS}, {"buffer", &FighterSpec::buffer, 1, MAX_HIT_TICKS}};
const char* const HEAD_TEXT[] = {"sheet", "chain"};

const FixKey<Strike> MOVE_FIX[] = {
    {"depth", &Strike::depth, DEPTH_LO, MAX_FIGHTER_DEPTH},
    {"knock_x", &Strike::knock_x, KNOCK_LO, MAX_FIGHTER_SPEED},
    {"knock_y", &Strike::knock_y, KNOCK_LO, MAX_FIGHTER_SPEED},
};
const TickKey<Strike> MOVE_TICK[] = {
    {"damage", &Strike::damage, 0, MAX_DAMAGE},
    {"hitstop", &Strike::hitstop, 0, MAX_HIT_TICKS},
    {"hitstun", &Strike::hitstun, 0, MAX_HIT_TICKS},
};
const char* const MOVE_TEXT[] = {"type", "hits_down"};
const char* const TYPE_NAMES[] = {"light", "heavy", "launch", "grab", "throw"};

constexpr uint32_t HEAD_KEYS = std::size(HEAD_FIX) + std::size(HEAD_TICK) + std::size(HEAD_TEXT);
constexpr uint32_t MOVE_KEYS = std::size(MOVE_FIX) + std::size(MOVE_TICK) + std::size(MOVE_TEXT);
static_assert(std::size(TYPE_NAMES) == HIT_TYPE_LAST + 1u);
static_assert(HEAD_KEYS <= 32 && MOVE_KEYS <= 32);

template <class T, std::size_t NF, std::size_t NT, std::size_t NS>
const char* key_name(const FixKey<T> (&fix)[NF], const TickKey<T> (&tick)[NT], const char* const (&text)[NS],
                     uint32_t i) {
    if (i < NF) return fix[i].name;
    if (i < NF + NT) return tick[i - NF].name;
    return text[i - NF - NT];
}

template <class T, std::size_t NF, std::size_t NT, std::size_t NS>
int key_index(const FixKey<T> (&fix)[NF], const TickKey<T> (&tick)[NT], const char* const (&text)[NS],
              const std::string& key) {
    for (uint32_t i = 0; i < NF + NT + NS; ++i)
        if (key == key_name(fix, tick, text, i)) return static_cast<int>(i);
    return -1;
}

template <class T, std::size_t NF, std::size_t NT>
bool set_number(T& obj, const FixKey<T> (&fix)[NF], const TickKey<T> (&tick)[NT], uint32_t i,
                const std::string& key, const std::string& value, int line, FighterBakeError& err) {
    if (i < NF) {
        fix32 v{};
        if (!core::parse_fix(value, v)) return fighter_fail(err, line, key + " must be a decimal number");
        if (v < fix[i].lo || fix[i].hi < v)
            return fighter_fail(err, line, key + " is outside the range the engine accepts");
        obj.*fix[i].field = v;
        return true;
    }
    uint32_t v = 0;
    if (!core::parse_u32(value, v)) return fighter_fail(err, line, key + " must be a whole number");
    if (v < tick[i - NF].lo || v > tick[i - NF].hi)
        return fighter_fail(err, line, key + " is outside the range the engine accepts");
    obj.*tick[i - NF].field = v;
    return true;
}

bool set_head(FighterSpec& f, uint32_t i, const std::string& key, const std::string& value, int line,
              FighterBakeError& err) {
    if (key == "chain") return parse_chain(value, line, f, err);
    if (key != "sheet") return set_number(f, HEAD_FIX, HEAD_TICK, i, key, value, line, err);
    if (!name_ok(value)) return fighter_fail(err, line, "sheet must be a name without '/' or control characters");
    f.sheet = value;
    f.sheet_line = line;
    return true;
}

bool set_move(Strike& s, uint32_t i, const std::string& key, const std::string& value, int line,
              FighterBakeError& err) {
    if (key == "hits_down") {
        if (value != "yes" && value != "no") return fighter_fail(err, line, "hits_down must be yes or no");
        s.hits_down = value == "yes";
        return true;
    }
    if (key != "type") return set_number(s, MOVE_FIX, MOVE_TICK, i, key, value, line, err);
    for (uint8_t t = 0; t <= HIT_TYPE_LAST; ++t)
        if (value == TYPE_NAMES[t]) {
            s.type = static_cast<HitType>(t);
            return true;
        }
    return fighter_fail(err, line, "type must be light, heavy, launch, grab or throw");
}

bool close_block(const FighterSpec& f, uint32_t seen, int line, FighterBakeError& err) {
    const bool head = f.moves.empty();
    for (uint32_t i = 0; i < (head ? HEAD_KEYS : MOVE_KEYS); ++i) {
        if ((seen & (1u << i)) != 0) continue;
        if (head) {
            const std::string key = key_name(HEAD_FIX, HEAD_TICK, HEAD_TEXT, i);
            return fighter_fail(err, line, "the fighter is missing " + key);
        }
        return fighter_fail(err, line, move_label(f.moves.back()) + " is missing " +
                                           key_name(MOVE_FIX, MOVE_TICK, MOVE_TEXT, i));
    }
    return true;
}

bool open_move(FighterSpec& f, const std::vector<std::string>& fields, int line, FighterBakeError& err) {
    const std::string& box = fields[2];
    if (box.size() != 4 || box.compare(0, 3, "hit") != 0 || box[3] < '0' || box[3] >= '0' + MAX_HIT_BOXES)
        return fighter_fail(err, line, "move box '" + box + "' must be hit0..hit3");
    if (!name_ok(fields[1]))
        return fighter_fail(err, line, "a move clip must be a name without '/' or control characters");
    MoveSpec m;
    m.clip = fields[1];
    m.line = line;
    m.strike.box = static_cast<uint8_t>(box[3] - '0');
    for (const MoveSpec& o : f.moves)
        if (o.clip == m.clip && o.strike.box == m.strike.box)
            return fighter_fail(err, line, "move '" + m.clip + "' " + box + " is declared twice");
    f.moves.push_back(m);
    return true;
}

} // namespace

bool parse_fighter(const std::string& text, FighterSpec& out, FighterBakeError& err) {
    out = FighterSpec{};
    uint32_t seen = 0;
    int line = 0, content = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        ++line;
        const std::size_t hash = raw.find('#');
        if (hash != std::string::npos) raw.erase(hash);
        const std::string body = core::trim(raw);
        if (body.empty()) continue;
        const int prev = content;
        content = line;
        const std::vector<std::string> f = core::split_fields(body);
        for (const std::string& field : f)
            if (field.empty()) return fighter_fail(err, line, "an empty field");
        if (f[0] == "move") {
            if (f.size() != 3) return fighter_fail(err, line, "expected 'move | <clip> | hit<N>'");
            if (!close_block(out, seen, prev > 0 ? prev : line, err) || !open_move(out, f, line, err))
                return false;
            seen = 0;
            continue;
        }
        if (f.size() != 2) return fighter_fail(err, line, "expected '<key> | <value>'");
        const bool head = out.moves.empty();
        const int i = head ? key_index(HEAD_FIX, HEAD_TICK, HEAD_TEXT, f[0])
                           : key_index(MOVE_FIX, MOVE_TICK, MOVE_TEXT, f[0]);
        if (i < 0) return fighter_fail(err, line, "unknown key '" + f[0] + "'" + (head ? "" : " in a move"));
        const uint32_t bit = 1u << static_cast<uint32_t>(i);
        if ((seen & bit) != 0) return fighter_fail(err, line, f[0] + " is set twice");
        seen |= bit;
        const bool ok = head ? set_head(out, static_cast<uint32_t>(i), f[0], f[1], line, err)
                             : set_move(out.moves.back().strike, static_cast<uint32_t>(i), f[0], f[1], line, err);
        if (!ok) return false;
    }
    if (content == 0) return fighter_fail(err, line, "the fighter file is empty");
    std::vector<uint32_t> chain;
    return close_block(out, seen, content, err) && chain_moves(out, chain, err);
}

} // namespace framework::brawl
