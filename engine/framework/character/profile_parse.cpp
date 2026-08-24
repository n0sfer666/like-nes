#include <cstring>

#include "profile_bake.hpp"
#include "profile_text.hpp"

// Грамматика манифеста: сколько полей в строке, что в них лежит, куда это класть и что нельзя
// решить по одной строке. Отделено от сборки байтов (`profile_bake.cpp`) по той же границе, что
// `preset_parse.cpp` от `preset_bake.cpp`: раскладка таблицы пиннута static_assert'ами и едет
// вместе с версией формата, а грамматика текста живёт своей жизнью.

namespace framework::character {
namespace {

// Ключи манифеста — ТАБЛИЦА, а не цепочка сравнений: список полей и список того, что каждое из них
// вправе принять, обязаны быть одним местом. Иначе поле, добавленное в `MoveProfile`, читается из
// манифеста, но не проверяется потолком — или наоборот.
struct FixKey {
    const char* name;
    fix32 MoveProfile::* field;
    fix32 limit;
};

const FixKey FIX_KEYS[] = {
    {"max_speed", &MoveProfile::max_speed, MAX_MOVE_SPEED},
    {"ground_accel", &MoveProfile::ground_accel, MAX_MOVE_ACCEL},
    {"ground_decel", &MoveProfile::ground_decel, MAX_MOVE_ACCEL},
    {"air_accel", &MoveProfile::air_accel, MAX_MOVE_ACCEL},
    {"air_decel", &MoveProfile::air_decel, MAX_MOVE_ACCEL},
    {"gravity_rise", &MoveProfile::gravity_rise, MAX_MOVE_ACCEL},
    {"gravity_fall", &MoveProfile::gravity_fall, MAX_MOVE_ACCEL},
    {"max_fall_speed", &MoveProfile::max_fall_speed, MAX_MOVE_SPEED},
    {"jump_height", &MoveProfile::jump_height, MAX_JUMP_HEIGHT},
    {"min_jump_height", &MoveProfile::min_jump_height, MAX_JUMP_HEIGHT},
    {"corner_correction", &MoveProfile::corner_correction, MAX_CORNER_CORRECTION},
    {"ground_snap", &MoveProfile::ground_snap, MAX_GROUND_SNAP},
};
constexpr uint32_t FIX_COUNT = sizeof(FIX_KEYS) / sizeof(FIX_KEYS[0]);

struct TickKey {
    const char* name;
    uint32_t MoveProfile::* field;
};

const TickKey TICK_KEYS[] = {
    {"coyote_ticks", &MoveProfile::coyote_ticks},
    {"buffer_ticks", &MoveProfile::buffer_ticks},
};
constexpr uint32_t TICK_COUNT = sizeof(TICK_KEYS) / sizeof(TICK_KEYS[0]);
constexpr uint32_t KEY_COUNT = FIX_COUNT + TICK_COUNT;
static_assert(KEY_COUNT <= 32, "the seen-mask is a uint32");

bool fail(ProfileBakeError& err, int line, const std::string& message) {
    err.line = line;
    err.message = message;
    return false;
}

// Закрытие профиля: то, о чём нельзя судить по одной строке. Недостающий ключ называется ПОИМЁННО
// и строкой, на которой профиль кончился, — «profile 'player' is missing max_speed» ведёт к правке,
// а «bad manifest» ведёт к чтению исходников пекаря.
bool close_profile(const std::vector<NamedProfile>& out, uint32_t seen, int line,
                   ProfileBakeError& err) {
    if (out.empty()) return true;
    const NamedProfile& p = out.back();
    for (uint32_t i = 0; i < KEY_COUNT; ++i) {
        if ((seen & (1u << i)) != 0) continue;
        const char* name = i < FIX_COUNT ? FIX_KEYS[i].name : TICK_KEYS[i - FIX_COUNT].name;
        return fail(err, line, "profile '" + p.name + "' is missing " + name);
    }
    if (p.profile.jump_height < p.profile.min_jump_height)
        return fail(err, line, "profile '" + p.name + "': min_jump_height is above jump_height");
    return true;
}

bool assign(NamedProfile& np, uint32_t& seen, const std::string& key, const std::string& value,
            int line, ProfileBakeError& err) {
    for (uint32_t i = 0; i < FIX_COUNT; ++i) {
        if (std::strcmp(FIX_KEYS[i].name, key.c_str()) != 0) continue;
        if ((seen & (1u << i)) != 0) return fail(err, line, key + " is set twice in this profile");
        fix32 v{};
        if (!profile_parse_fix(value, v)) return fail(err, line, key + " must be a decimal number");
        if (v.raw < 0 || FIX_KEYS[i].limit < v)
            return fail(err, line, key + " is outside the range the engine accepts");
        np.profile.*FIX_KEYS[i].field = v;
        seen |= 1u << i;
        return true;
    }
    for (uint32_t i = 0; i < TICK_COUNT; ++i) {
        if (std::strcmp(TICK_KEYS[i].name, key.c_str()) != 0) continue;
        const uint32_t bit = 1u << (FIX_COUNT + i);
        if ((seen & bit) != 0) return fail(err, line, key + " is set twice in this profile");
        uint32_t v = 0;
        if (!profile_parse_u32(value, v))
            return fail(err, line, key + " must be a whole number of ticks");
        if (v > MAX_WINDOW_TICKS)
            return fail(err, line, key + " is outside the range the engine accepts");
        np.profile.*TICK_KEYS[i].field = v;
        seen |= bit;
        return true;
    }
    return fail(err, line, "unknown key '" + key + "'");
}

} // namespace

bool parse_profiles(const std::string& text, std::vector<NamedProfile>& out,
                    ProfileBakeError& err) {
    out.clear();
    uint32_t seen = 0;
    int line = 0;
    // Номер строки для отказа «на закрытии профиля» берётся у последней СОДЕРЖАТЕЛЬНОЙ строки, а не
    // у счётчика: манифест обычно кончается переводом строки, и счётчик показывал бы на строку ЗА
    // концом файла. Отказ, ведущий в несуществующую строку, ведёт ровно туда, куда номер строки и
    // существует, чтобы не пускать.
    int content_line = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        std::string raw = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        ++line;
        const std::size_t hash = raw.find('#');
        if (hash != std::string::npos) raw.erase(hash);
        const std::string body = profile_trim(raw);
        if (body.empty()) continue;
        const int prev_content = content_line;
        content_line = line;

        const std::vector<std::string> f = profile_split(body);
        if (f.size() != 2 || f[0].empty() || f[1].empty())
            return fail(err, line, "expected '<key> | <value>'");
        if (f[0] == "profile") {
            if (!close_profile(out, seen, prev_content > 0 ? prev_content : line, err)) return false;
            for (const NamedProfile& p : out)
                if (p.name == f[1]) return fail(err, line, "profile '" + f[1] + "' is declared twice");
            out.push_back(NamedProfile{f[1], MoveProfile{}});
            seen = 0;
            continue;
        }
        if (out.empty()) return fail(err, line, "a value before the first 'profile' line");
        if (!assign(out.back(), seen, f[0], f[1], line, err)) return false;
    }
    const int end_line = content_line > 0 ? content_line : line;
    if (!close_profile(out, seen, end_line, err)) return false;
    // Пустой манифест — находка, а не законный «ноль профилей»: таблица без строк читается без
    // ошибки и означает «персонаж не настроен», то есть отдаёт отладку в рантайм.
    if (out.empty()) return fail(err, end_line, "the manifest declares no profile");
    return true;
}

} // namespace framework::character
