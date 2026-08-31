#include "rebind_store.hpp"

#include <cstdio>
#include <cstdlib>

#include "platform_fs.hpp"
#include "preset_parse.hpp"
#include "source_names.hpp"

namespace framework::input {
namespace {

bool parse_index(const std::string& s, uint32_t& out) {
    if (s.empty() || s.size() > 3) return false;
    uint32_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + static_cast<uint32_t>(c - '0');
    }
    out = v;
    return true;
}

bool write_atomic(const std::string& path, const std::string& text) {
    const std::string tmp = path + ".tmp";
    std::FILE* f = platform::open_file(tmp, "wb");
    if (f == nullptr) return false;
    const bool written = text.empty() || std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool synced = written && platform::sync_file(f);
    const bool closed = std::fclose(f) == 0;
    if (!written || !synced || !closed || !platform::replace_file(tmp, path)) {
        platform::remove_file(tmp);
        return false;
    }
    platform::sync_dir_of(path);
    return true;
}

} // namespace

void RebindStore::set(const std::string& action, uint32_t which, ::input::Source src) {
    for (Rebind& r : items_)
        if (r.action == action && r.which == which) {
            r.src = src;
            return;
        }
    items_.push_back({action, which, src});
}

bool RebindStore::get(const std::string& action, uint32_t which, ::input::Source& out) const {
    for (const Rebind& r : items_)
        if (r.action == action && r.which == which) {
            out = r.src;
            return true;
        }
    return false;
}

void RebindStore::reset(const std::string& action) {
    for (auto it = items_.begin(); it != items_.end();)
        it->action == action ? it = items_.erase(it) : ++it;
}

void RebindStore::apply(const PresetTable& table, uint32_t preset, ::input::ActionMap& map) const {
    for (const Rebind& r : items_) {
        const int action = table.find_action(preset, r.action.c_str());
        if (action < 0) continue;
        map.rebind(action, static_cast<int>(r.which), r.src);
    }
}

std::string RebindStore::serialize(const std::string& preset) const {
    std::string out = "preset | " + preset + "\n";
    for (const Rebind& r : items_) {
        std::string name = source_name(r.src);
        // Снятый биндинг — такая же осознанная правка игрока, как назначенный, и обязан пережить
        // перезапуск: пропусти его при записи — и кнопка вернулась бы из пресета сама собой.
        if (name.empty()) name = "none";
        out += "bind | " + r.action + " | " + std::to_string(r.which) + " | " + name + "\n";
    }
    return out;
}

bool RebindStore::parse(const std::string& text, std::string& preset_out) {
    // Разбор идёт во ВРЕМЕННУЮ накладку: отказ на середине файла обязан оставить игрока с чистым
    // пресетом, а не с первой половиной чужих правок.
    RebindStore parsed;
    preset_out.clear();
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? nl : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        const std::vector<std::string> f = core::split_fields(line);
        if (f.size() == 1 && f[0].empty()) continue;
        if (!f[0].empty() && f[0][0] == '#') continue;
        if (f[0] == "preset" && f.size() == 2) {
            preset_out = f[1];
            continue;
        }
        if (f[0] != "bind" || f.size() != 4) return false;
        uint32_t which = 0;
        ::input::Source src;
        if (!parse_index(f[2], which)) return false;
        if (f[3] != "none" && !parse_source(f[3], src)) return false;
        parsed.set(f[1], which, src);
    }
    if (preset_out.empty()) return false;
    items_ = parsed.items_;
    return true;
}

bool RebindStore::save(const std::string& path, const std::string& preset) const {
    return write_atomic(path, serialize(preset));
}

bool RebindStore::load(const std::string& path, std::string& preset_out) {
    std::string text;
    if (!platform::read_text(path, text)) {
        preset_out.clear();
        return false;
    }
    return parse(text, preset_out);
}

} // namespace framework::input
