#include "rumble_fighter.hpp"

#include <cstdio>
#include <cstring>

#include "clip.hpp"
#include "hash.hpp"
#include "object_read.hpp"
#include "rumble_level.hpp"

namespace rumble {

namespace {

using framework::tilemap::ObjectMap;

struct Turn {
    const char* clip;
    uint64_t ticks;
};

constexpr Turn SHOWCASE[] = {{"queen/Walk", 60}, {"queen/Jab", 30}, {"queen/Hook", 30}, {"queen/Uppercut", 30}};

constexpr uint64_t round_ticks() {
    uint64_t sum = 0;
    for (const Turn& t : SHOWCASE) sum += t.ticks;
    return sum;
}

bool table(const Level& level, const char* name, const uint8_t*& data, size_t& size) {
    const asset::LookupFault f = asset::raw_table(level.bundle, asset::fnv1a_str(name), data, size);
    if (f == asset::LookupFault::Ok) return true;
    std::fprintf(stderr, "neon-rumble: %s table: %s\n", name, asset::lookup_fault_name(f));
    return false;
}

bool opened(bool ok, const char* name) {
    if (!ok) std::fprintf(stderr, "neon-rumble: %s table: does not open\n", name);
    return ok;
}

bool find_spawn(const Level& level, Fighter& out) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    framework::tilemap::ObjectTable objects;
    if (!table(level, "objects", data, size) || !opened(objects.open(data, size), "objects")) return false;
    const ObjectMap map = objects.find(level.name);
    for (const framework::tilemap::MapObject& o : framework::tilemap::objects_of_class(map, "spawn")) {
        if (std::strcmp(framework::tilemap::object_text(map, o.name_offset), "player") != 0) continue;
        out.spawn = {fix32::from_raw(o.x_raw), fix32::from_raw(o.y_raw)};
        const framework::tilemap::ObjectProp* facing = framework::tilemap::object_prop(map, o, "facing");
        out.faces_left = facing != nullptr && facing->type == static_cast<uint8_t>(framework::tilemap::PropType::String) &&
                         std::strcmp(framework::tilemap::object_text(map, static_cast<uint32_t>(facing->value)), "left") == 0;
        return true;
    }
    std::fprintf(stderr, "neon-rumble: level %s has no spawn named player\n", level.name);
    return false;
}

} // namespace

bool Fighter::open(const Level& level) {
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!table(level, "clips", data, size) || !opened(clips.open(data, size), "clips")) return false;
    for (const Turn& t : SHOWCASE)
        if (clips.find(t.clip).row == nullptr) {
            std::fprintf(stderr, "neon-rumble: no clip %s in the clips table\n", t.clip);
            return false;
        }
    const uint64_t guid = clips.find(SHOWCASE[0].clip).row->texture_guid;
    for (const Turn& t : SHOWCASE)
        if (clips.find(t.clip).row->texture_guid != guid) {
            std::fprintf(stderr, "neon-rumble: clip %s is on another sheet than %s\n", t.clip, SHOWCASE[0].clip);
            return false;
        }
    const asset::LookupFault f = asset::raw_rgba8(level.bundle, guid, sheet);
    if (f != asset::LookupFault::Ok) {
        std::fprintf(stderr, "neon-rumble: fighter sheet %016llx: %s\n", static_cast<unsigned long long>(guid),
                     asset::lookup_fault_name(f));
        return false;
    }
    sheet_size = {sheet.width, sheet.height};
    return find_spawn(level, *this);
}

Pose Fighter::pose(uint64_t tick) const {
    const uint64_t round = tick / round_ticks();
    uint64_t local = tick % round_ticks();
    const Turn* turn = SHOWCASE;
    while (local >= turn->ticks) local -= (turn++)->ticks;
    Pose p;
    p.name = turn->clip;
    p.clip = clips.find(turn->clip);
    p.frame = framework::graphics::clip_frame_at(p.clip.clip, local);
    p.flip = faces_left != (round % 2 == 1);
    return p;
}

} // namespace rumble
