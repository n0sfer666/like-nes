#include <string>

#include "hash.hpp"
#include "level_parts.hpp"

namespace framework::scene {

namespace {

bool known(const Level& l, std::string_view kind) {
    for (uint32_t i = 0; i < l.fighter_count; ++i)
        if (l.fighters[i].kind == kind) return true;
    return false;
}

std::string spawn_text(const Level& l, const WaveSpawn& s) {
    return std::string("level ") + l.name + ": spawn " + tilemap::object_text(l.object_map, s.mark.object->name_offset) +
           " kind " + std::string(s.kind);
}

bool open_fighter(const Level& l, const WaveSpawn& s, brawl::FighterTable& out, LoadError& error) {
    const std::string table = std::string(s.kind) + "_fighter";
    const uint8_t* data = nullptr;
    std::size_t size = 0;
    const asset::LookupFault f = asset::raw_table(l.bundle, asset::fnv1a_str(table.c_str()), data, size);
    if (f == asset::LookupFault::Missing)
        return load_fail(error, LoadFault::UnknownKind, spawn_text(l, s) + ": no table " + table);
    if (f != asset::LookupFault::Ok)
        return load_fail(error, LoadFault::BadFighter, table + " table: " + asset::lookup_fault_name(f));
    if (!out.open(data, size)) return load_fail(error, LoadFault::BadFighter, table + " table does not open");
    for (uint32_t i = 0; i < out.move_count(); ++i)
        if (l.clips.find(out.move_clip(i)).row == nullptr)
            return load_fail(error, LoadFault::BadFighter,
                             table + ": no clip " + out.move_clip(i) + " in the clips table");
    return true;
}

} // namespace

bool load_fighters(Level& level, LoadError& error) {
    for (const WaveSpawn& s : level.objects.waves) {
        if (known(level, s.kind)) continue;
        if (level.fighter_count == LEVEL_FIGHTERS)
            return load_fail(error, LoadFault::Capacity,
                             std::string("level ") + level.name + " needs more than " +
                                 std::to_string(LEVEL_FIGHTERS) + " fighter kinds");
        LevelFighter& f = level.fighters[level.fighter_count];
        if (!open_fighter(level, s, f.table, error)) return false;
        f.kind = s.kind;
        ++level.fighter_count;
    }
    return true;
}

} // namespace framework::scene
