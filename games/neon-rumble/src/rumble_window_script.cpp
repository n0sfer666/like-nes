#include "rumble_window_script.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <charconv>
#include <optional>
#include <sstream>
#include <utility>

#include "platform_fs.hpp"
#include "rumble_coop_script.hpp"

namespace rumble {

namespace {

constexpr size_t MAX_SCRIPT_BYTES = 1u << 20;
constexpr uint32_t MAX_AT = 1'000'000;

struct NamedKey {
    const char* name;
    int code;
};

constexpr NamedKey NAMED_KEYS[] = {
    {"SPACE", GLFW_KEY_SPACE},   {"ENTER", GLFW_KEY_ENTER},       {"ESCAPE", GLFW_KEY_ESCAPE},
    {"BACKSPACE", GLFW_KEY_BACKSPACE}, {"RIGHT", GLFW_KEY_RIGHT}, {"LEFT", GLFW_KEY_LEFT},
    {"DOWN", GLFW_KEY_DOWN},     {"UP", GLFW_KEY_UP},             {"F1", GLFW_KEY_F1},
    {"F3", GLFW_KEY_F3},         {"KP_SUBTRACT", GLFW_KEY_KP_SUBTRACT},
};

std::optional<uint16_t> key_code(const std::string& name) {
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') return static_cast<uint16_t>(name[0]);
    if (name.size() == 3 && name.starts_with("KP") && name[2] >= '0' && name[2] <= '9')
        return static_cast<uint16_t>(GLFW_KEY_KP_0 + (name[2] - '0'));
    for (const NamedKey& k : NAMED_KEYS)
        if (name == k.name) return static_cast<uint16_t>(k.code);
    return std::nullopt;
}

bool fail(std::string& why, std::string text) {
    why = std::move(text);
    return false;
}

bool parse_at(const std::string& text, uint32_t& at) {
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, at);
    return ec == std::errc() && ptr == end && at <= MAX_AT;
}

bool frame_name_ok(const std::string& name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

} // namespace

bool WindowScript::open(const std::string& path, std::string& why) {
    std::vector<uint8_t> bytes;
    if (!platform::read_bytes_capped(path, bytes, MAX_SCRIPT_BYTES)) {
        why = "cannot read " + path;
        return false;
    }
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string line;
    for (uint32_t n = 1; std::getline(in, line); ++n) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!parse(line, why)) {
            why = path + ":" + std::to_string(n) + ": " + why;
            return false;
        }
    }
    const auto by_time = [](const Step& a, const Step& b) { return a.at < b.at; };
    std::stable_sort(menu_.steps.begin(), menu_.steps.end(), by_time);
    for (Track& t : levels_) std::stable_sort(t.steps.begin(), t.steps.end(), by_time);
    const auto quits = [](const Track& t) {
        return std::any_of(t.steps.begin(), t.steps.end(), [](const Step& s) { return s.verb == Verb::Quit; });
    };
    if (quits(menu_) || std::any_of(levels_.begin(), levels_.end(), quits)) return true;
    why = path + ": no 'quit' step, the window would never close";
    return false;
}

bool WindowScript::parse(const std::string& line, std::string& why) {
    std::istringstream words(line);
    std::string first, extra;
    if (!(words >> first) || first[0] == '#' || first[0] == '!') return true;
    if (first == "level") {
        if (words >> extra) return fail(why, "extra word '" + extra + "' after 'level'");
        levels_.emplace_back();
        return true;
    }
    const bool menu = first == "menu";
    std::string at_text = first;
    if (menu && !(words >> at_text)) at_text.clear();
    std::string verb, arg;
    words >> verb >> arg;
    if (words >> extra) return fail(why, "extra word '" + extra + "'");
    uint32_t at = 0;
    if (!parse_at(at_text, at)) return fail(why, "bad time '" + at_text + "', want 0.." + std::to_string(MAX_AT));
    if (!menu && levels_.empty()) return fail(why, "a level step before the first 'level' line");
    Track& track = menu ? menu_ : levels_.back();
    if (verb == "quit") {
        if (!arg.empty()) return fail(why, "extra word '" + arg + "' after 'quit'");
        track.steps.push_back(Step{at, Verb::Quit, 0, {}});
        return true;
    }
    if (verb == "shot") return add_shot(track, at, arg, why);
    if (verb != "down" && verb != "up" && verb != "tap") return fail(why, "unknown verb '" + verb + "'");
    const std::optional<uint16_t> key = key_code(arg);
    if (!key) return fail(why, "unknown key '" + arg + "'");
    if (verb != "up") track.steps.push_back(Step{at, Verb::Down, *key, {}});
    if (verb != "down") track.steps.push_back(Step{verb == "tap" ? at + 1 : at, Verb::Up, *key, {}});
    return true;
}

bool WindowScript::add_shot(Track& track, uint32_t at, const std::string& name, std::string& why) {
    if (name.empty()) return fail(why, "'shot' needs a frame name");
    if (!frame_name_ok(name)) return fail(why, "frame name '" + name + "' is not [A-Za-z0-9_-]+");
    std::string folded = name;
    std::transform(folded.begin(), folded.end(), folded.begin(),
                   [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
    if (std::find(names_.begin(), names_.end(), folded) != names_.end())
        return fail(why, "frame name '" + name + "' is used twice, letter case aside");
    names_.push_back(folded);
    track.steps.push_back(Step{at, Verb::Shot, 0, name});
    return true;
}

void WindowScript::enter_level() { ++entered_; }

void WindowScript::feed(bool level, uint32_t at, input::InputEngine& engine, WindowKeys& keys) {
    shots_.clear();
    const size_t now = level ? entered_ : 0;
    if (now != fed_) {
        for (const uint16_t k : held_) post_key(engine, k, false, at);
        held_.clear();
        fed_ = now;
    }
    if (level && (entered_ == 0 || entered_ > levels_.size())) return;
    Track& track = level ? levels_[entered_ - 1] : menu_;
    while (track.next < track.steps.size() && track.steps[track.next].at <= at) {
        const Step& s = track.steps[track.next++];
        if (s.verb == Verb::Shot) {
            shots_.push_back(s.name);
        } else if (s.verb == Verb::Quit) {
            quit_ = true;
        } else {
            post_key(engine, s.key, s.verb == Verb::Down, at);
            std::erase(held_, s.key);
            if (s.verb == Verb::Down) {
                keys.press_code(s.key);
                held_.push_back(s.key);
            }
        }
    }
}

} // namespace rumble
