#include <cstring>

#include "platform_fs.hpp"
#include "profile_bake.hpp"
#include "profile_format.hpp"

// Сборка байтов таблицы: заголовок, строки, блоб имён. Грамматика манифеста — в
// `profile_parse.cpp`.
namespace framework::character {
namespace {

void put(std::vector<uint8_t>& out, const void* p, std::size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

} // namespace

bool bake_profiles(const std::string& text, std::vector<uint8_t>& out, ProfileBakeError& err) {
    std::vector<NamedProfile> profiles;
    if (!parse_profiles(text, profiles, err)) return false;

    std::vector<char> blob{'\0'};   // смещение 0 — пустая строка, значит «имени нет»
    std::vector<MoveRow> rows;
    rows.reserve(profiles.size());
    // Дедупа имён здесь нет намеренно: повтор имени профиля отвергает `parse_profiles`, и ветка
    // «имя уже в блобе» была бы кодом, который не может выполниться — в пути, который печёт байты
    // голдена. У таблицы пресетов дедуп осмыслен по своей причине: там повторяются имена ДЕЙСТВИЙ,
    // а не профилей, и повтор законен.
    for (const NamedProfile& np : profiles) {
        const auto off = static_cast<uint32_t>(blob.size());
        blob.insert(blob.end(), np.name.begin(), np.name.end());
        blob.push_back('\0');
        const MoveProfile& p = np.profile;
        MoveRow r{};
        r.name_offset = off;
        r.max_speed_raw = p.max_speed.raw;
        r.ground_accel_raw = p.ground_accel.raw;
        r.ground_decel_raw = p.ground_decel.raw;
        r.air_accel_raw = p.air_accel.raw;
        r.air_decel_raw = p.air_decel.raw;
        r.gravity_rise_raw = p.gravity_rise.raw;
        r.gravity_fall_raw = p.gravity_fall.raw;
        r.max_fall_speed_raw = p.max_fall_speed.raw;
        r.jump_height_raw = p.jump_height.raw;
        r.min_jump_height_raw = p.min_jump_height.raw;
        r.coyote_ticks = p.coyote_ticks;
        r.buffer_ticks = p.buffer_ticks;
        r.corner_correction_raw = p.corner_correction.raw;
        r.ground_snap_raw = p.ground_snap.raw;
        r.max_slope_raw = p.max_slope.raw;
        rows.push_back(r);
    }

    MoveHeader h{};
    std::memcpy(h.magic, MOVE_MAGIC, sizeof(h.magic));
    h.version = MOVE_VERSION;
    h.profile_count = static_cast<uint32_t>(rows.size());
    h.profiles_offset = static_cast<uint32_t>(sizeof(MoveHeader));
    h.strings_offset = h.profiles_offset + static_cast<uint32_t>(rows.size() * sizeof(MoveRow));
    h.total_size = h.strings_offset + static_cast<uint32_t>(blob.size());

    out.clear();
    out.reserve(h.total_size);
    put(out, &h, sizeof(h));
    for (const MoveRow& r : rows) put(out, &r, sizeof(r));
    put(out, blob.data(), blob.size());
    return true;
}

bool bake_profiles_file(const std::string& path, std::vector<uint8_t>& out,
                        ProfileBakeError& err) {
    std::string text;
    if (!platform::read_text(path, text)) {
        err.line = 0;
        err.message = "cannot read " + path;
        return false;
    }
    return bake_profiles(text, out, err);
}

} // namespace framework::character
