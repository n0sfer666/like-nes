#include "preset_parse.hpp"

// Профиль пада — вторая сущность манифеста и вторая ответственность: раскладка железа не зависит
// ни от одного пресета, поэтому её разбор живёт отдельно от разбора действий и осей.
namespace framework::input {
namespace {

bool parse_labels(const std::string& s, uint32_t& out) {
    if (s == "xbox") { out = 0; return true; }
    if (s == "nintendo") { out = 1; return true; }
    if (s == "playstation") { out = 2; return true; }
    return false;
}

bool parse_hex16(const std::string& s, uint32_t& out) {
    if (s == "-") { out = 0; return true; }
    if (s.empty() || s.size() > 6) return false;
    std::size_t i = 0;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
    uint32_t v = 0;
    for (; i < s.size(); ++i) {
        const char ch = s[i];
        uint32_t d;
        if (ch >= '0' && ch <= '9') d = static_cast<uint32_t>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') d = static_cast<uint32_t>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') d = static_cast<uint32_t>(ch - 'A' + 10);
        else return false;
        v = v * 16 + d;
    }
    if (v > 0xFFFF) return false;
    out = v;
    return true;
}

} // namespace

bool preset_parse_pad(PresetBuild& b, const std::vector<std::string>& f, int line, PresetBakeError& err) {
    if (f.size() != 8)
        return preset_fail(err, line,
                    "pad needs a name, a vid, a pid, a name match, a label set, a deadzone and "
                    "a trigger threshold");
    PadRow r{};
    r.name_offset = b.blob.add(f[1]);
    if (!parse_hex16(f[2], r.vid) || !parse_hex16(f[3], r.pid))
        return preset_fail(err, line, "vid and pid must be hex numbers or '-'");
    r.match_offset = f[4] == "-" ? 0 : b.blob.add(f[4]);
    if (r.vid == 0 && r.match_offset == 0)
        return preset_fail(err, line, "a pad row with no vid must match by name, otherwise it can never "
                               "be selected");
    if (!parse_labels(f[5], r.labels))
        return preset_fail(err, line, "the label set must be xbox, nintendo or playstation");
    fix32 dz, trig;
    if (!core::parse_fix(f[6], dz) || !core::parse_fix(f[7], trig))
        return preset_fail(err, line, "the deadzone and the trigger threshold must be decimal numbers");
    r.deadzone_raw = dz.raw;
    r.outer_raw = fix32::ONE;
    r.curve_exp = 1;
    r.trigger_raw = trig.raw;
    b.pads.push_back(r);
    return true;
}


} // namespace framework::input
