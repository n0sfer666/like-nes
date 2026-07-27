#include "preset_bake.hpp"

#include <cstring>

#include "platform_fs.hpp"
#include "preset_format.hpp"
#include "preset_parse.hpp"

namespace framework::input {
namespace {

template <typename T>
void append(std::vector<uint8_t>& out, const std::vector<T>& rows) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(rows.data());
    out.insert(out.end(), bytes, bytes + rows.size() * sizeof(T));
}

} // namespace

bool bake_presets(const std::string& text, std::vector<uint8_t>& out, PresetBakeError& err) {
    PresetBuild b;
    int line = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        ++line;
        const std::size_t hash = raw.find('#');
        const std::string body = preset_trim(hash == std::string::npos ? raw : raw.substr(0, hash));
        if (body.empty()) continue;
        if (!preset_parse_line(b, preset_split(body), line, err)) return false;
    }
    if (!preset_close(b, err, line)) return false;
    if (b.presets.empty()) {
        err.line = 0;
        err.message = "the manifest declares no presets";
        return false;
    }

    PresetHeader h{};
    std::memcpy(h.magic, PRESET_MAGIC, sizeof(h.magic));
    h.version = PRESET_VERSION;
    h.preset_count = static_cast<uint32_t>(b.presets.size());
    h.action_count = static_cast<uint32_t>(b.actions.size());
    h.axis_count = static_cast<uint32_t>(b.axes.size());
    h.binding_count = static_cast<uint32_t>(b.bindings.size());
    h.presets_offset = sizeof(PresetHeader);
    h.actions_offset = h.presets_offset + static_cast<uint32_t>(b.presets.size() * sizeof(PresetRow));
    h.axes_offset = h.actions_offset + static_cast<uint32_t>(b.actions.size() * sizeof(ActionRow));
    h.bindings_offset = h.axes_offset + static_cast<uint32_t>(b.axes.size() * sizeof(AxisRow));
    h.strings_offset = h.bindings_offset + static_cast<uint32_t>(b.bindings.size() * sizeof(BindingRow));
    h.total_size = h.strings_offset + static_cast<uint32_t>(b.blob.data.size());

    out.clear();
    out.reserve(h.total_size);
    const auto* head = reinterpret_cast<const uint8_t*>(&h);
    out.insert(out.end(), head, head + sizeof(h));
    append(out, b.presets);
    append(out, b.actions);
    append(out, b.axes);
    append(out, b.bindings);
    out.insert(out.end(), b.blob.data.begin(), b.blob.data.end());
    return true;
}

bool bake_presets_file(const std::string& path, std::vector<uint8_t>& out, PresetBakeError& err) {
    std::string text;
    if (!platform::read_text(path, text)) {
        err.line = 0;
        err.message = "cannot read " + path;
        return false;
    }
    return bake_presets(text, out, err);
}

} // namespace framework::input
