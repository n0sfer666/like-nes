#include "touch_layout.hpp"

#include <algorithm>

#include "text_fields.hpp"

namespace framework::touch {
namespace {

using core::parse_fix;

bool refuse(std::string& error, const std::string& message) {
    error = message;
    return false;
}

const char* const ANCHORS[] = {"bottom-left", "bottom-right", "top-left", "top-right"};

bool anchor_of(const std::string& s, Anchor& out) {
    for (uint32_t i = 0; i < 4; ++i)
        if (s == ANCHORS[i]) {
            out = static_cast<Anchor>(i);
            return true;
        }
    return false;
}

bool button_of(const std::string& s, uint32_t& out) {
    if (s.size() != 1 || s[0] < 'a' || s[0] >= static_cast<char>('a' + BUTTONS)) return false;
    out = static_cast<uint32_t>(s[0] - 'a');
    return true;
}

bool in_range(fix32 v, int32_t lo, int32_t hi) {
    return !(v < fix32::from_int(lo)) && !(fix32::from_int(hi) < v);
}

bool radius_ok(fix32 r) { return fix32{} < r && in_range(r, 0, MAX_RADIUS_PT); }

const std::string STICK_RADIUS = "stick radius is outside (0, " + std::to_string(MAX_RADIUS_PT) + "] points";

bool button_ok(const ButtonSpec& b, uint32_t& used, std::string& why) {
    if (b.button >= BUTTONS) return refuse(why, "a button outside a..e");
    const std::string name(1, static_cast<char>('a' + b.button));
    if ((used >> b.button) & 1u) return refuse(why, "button '" + name + "' is placed twice");
    used |= 1u << b.button;
    if (static_cast<uint32_t>(b.anchor) > static_cast<uint32_t>(Anchor::TopRight))
        return refuse(why, "button '" + name + "' has an unknown anchor");
    if (!radius_ok(b.radius))
        return refuse(why, "button '" + name + "' radius is outside (0, " + std::to_string(MAX_RADIUS_PT) + "] points");
    if (!in_range(b.dx, 0, MAX_OFFSET_PT) || !in_range(b.dy, 0, MAX_OFFSET_PT))
        return refuse(why, "button '" + name + "' offset is outside [0, " + std::to_string(MAX_OFFSET_PT) + "] points");
    return true;
}

bool stick_line(const std::vector<std::string>& f, bool& seen, Layout& out, std::string& why) {
    if (f.size() != 2) return refuse(why, "a stick line is 'stick | <radius>'");
    if (seen) return refuse(why, "a second stick line (the layout has one stick)");
    if (!parse_fix(f[1], out.stick_radius)) return refuse(why, "stick radius '" + f[1] + "' is not a number");
    if (!radius_ok(out.stick_radius)) return refuse(why, STICK_RADIUS);
    seen = true;
    return true;
}

bool button_line(const std::vector<std::string>& f, uint32_t& used, Layout& out, std::string& why) {
    if (f.size() != 6) return refuse(why, "a button line is 'button | <a..e> | <anchor> | <dx> | <dy> | <radius>'");
    ButtonSpec b;
    if (!button_of(f[1], b.button)) return refuse(why, "button '" + f[1] + "' is not one of a, b, c, d, e");
    if (!anchor_of(f[2], b.anchor))
        return refuse(why, "anchor '" + f[2] + "' is not bottom-left, bottom-right, top-left or top-right");
    if (!parse_fix(f[3], b.dx) || !parse_fix(f[4], b.dy) || !parse_fix(f[5], b.radius))
        return refuse(why, "button '" + f[1] + "' has a field that is not a number");
    if (!button_ok(b, used, why)) return false;
    out.buttons[out.button_count++] = b;
    return true;
}

} // namespace

bool layout_ok(const Layout& layout, std::string& error) {
    if (!radius_ok(layout.stick_radius)) return refuse(error, STICK_RADIUS);
    if (layout.button_count > BUTTONS) return refuse(error, "more than five buttons");
    uint32_t used = 0;
    for (uint32_t i = 0; i < layout.button_count; ++i)
        if (!button_ok(layout.buttons[i], used, error)) return false;
    return true;
}

bool parse_layout(std::string_view text, const std::string& file, Layout& out, std::string& error) {
    out = Layout{};
    if (text.substr(0, 3) == "\xEF\xBB\xBF") return refuse(error, file + ":1: a byte order mark is not allowed");
    Layout l;
    bool stick = false;
    uint32_t used = 0;
    std::size_t start = 0;
    for (std::size_t line = 1; start < text.size(); ++line) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        const std::string raw = core::trim(std::string(text.substr(start, end - start)));
        start = end + 1;
        if (raw.empty() || raw[0] == '#') continue;
        const std::vector<std::string> f = core::split_fields(raw);
        std::string why;
        bool ok = false;
        if (f[0] == "stick") ok = stick_line(f, stick, l, why);
        else if (f[0] == "button") ok = button_line(f, used, l, why);
        else why = "unknown line kind '" + f[0] + "' (expected stick, button)";
        if (!ok) return refuse(error, file + ":" + std::to_string(line) + ": " + why);
    }
    if (!stick) return refuse(error, file + ": no stick line");
    out = l;
    return true;
}

} // namespace framework::touch
