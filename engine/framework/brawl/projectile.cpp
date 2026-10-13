#include "projectile.hpp"

#include "body_kind.hpp"
#include "hit_geometry.hpp"
#include "platform_world.hpp"
#include "section_mode.hpp"
#include "shape.hpp"

namespace framework::brawl {

namespace {

int8_t facing_of(fix32 vx, int8_t owner) {
    if (vx < fix32{}) return -1;
    return fix32{} < vx ? 1 : owner;
}

bool crosses(const scene::LevelObjects& level, fix32 from, fix32 to, scene::SectionMode keep, fix32& edge) {
    const bool right = from < to;
    bool any = false;
    for (const scene::Section& s : level.sections)
        for (const fix32 p : {s.area.rect.x0, s.area.rect.x1}) {
            const bool passed = right ? from < p && !(to < p) : to < p && !(from < p);
            if (!passed || mode_at(level, right ? p : before(p)) == keep) continue;
            const fix32 e = right ? before(p) : p;
            if (!any || (right ? e < edge : edge < e)) edge = e;
            any = true;
        }
    return any;
}

const scene::LevelObjects* modes(const BrawlWorld& w) { return w.platform != nullptr ? w.level : nullptr; }

scene::SectionMode keep_of(bool platform) { return platform ? scene::SectionMode::Platform : scene::SectionMode::Belt; }

bool own_side(const scene::LevelObjects& level, fix32 owner, bool platform, fix32& x) {
    const scene::SectionMode keep = keep_of(platform);
    fix32 edge;
    if (mode_at(level, x) == keep) return false;
    x = crosses(level, owner, x, keep, edge) ? edge : refusal_x(level, x, keep);
    return true;
}

void stop(Body& b) {
    b.pos.vx = fix32{};
    b.pos.vz = fix32{};
    b.pos.vy = fix32{};
    b.life = 1;
}

bool clamp_to_mode(Body& b, const DepthBody& from, const BrawlWorld& w, bool platform) {
    fix32 edge;
    if (modes(w) == nullptr || !crosses(*w.level, from.x, b.pos.x, keep_of(platform), edge)) return false;
    const fix32 f = (edge - from.x) / from.vx;
    b.pos.x = edge;
    b.pos.z = from.z + from.vz * f;
    b.pos.y = from.y + from.vy * f;
    return true;
}

constexpr graphics::ClipBox POINT{static_cast<uint8_t>(graphics::BoxKind::Hit), 0, 0, graphics::Rect16{0, -1, 1, 1}};

bool stop_on_floor(Body& b, const DepthBody& from, const DepthFloor& f) {
    const DepthBody to = b.pos;
    b.pos.x = move_x(f, from.x, from.z, to.x);
    b.pos.z = move_z(f, b.pos.x, from.z, to.z);
    b.pos.y = max_fix(to.y, fix32{});
    return !(b.pos.x == to.x) || !(b.pos.z == to.z) || !(b.pos.y == to.y);
}

fix32 screen_y(const DepthBody& d) { return d.z - d.y; }

bool scene_fraction(const Body& b, const DepthBody& from, const graphics::ClipBox& box, const PlatformWorld& p,
                    fix32& fraction) {
    character::CollisionScene scene = p.scene;
    scene.tiles.exclude = static_cast<tilemap::TileFlags>(scene.tiles.exclude | tilemap::TILE_ONEWAY);
    Body was = b;
    was.pos = from;
    const HitRect r = place_box(was, box.rect);
    const fix32 cx = mid_fix(r.x0, r.x1);
    const fix32 cy = from.z - mid_fix(r.h0, r.h1);
    const character::CharacterHull hull{physics::sanitize(physics::box(cx - r.x0, mid_fix(r.h0, r.h1) - r.h0)), p.hull.mask};
    const Vec2 travel{b.pos.x - from.x, screen_y(b.pos) - screen_y(from)};
    character::SceneHit hit;
    if (!character::cast_nearest(scene, hull, Vec2{cx, cy}, travel, hit)) return false;
    fraction = hit.fraction;
    return true;
}

bool stop_in_scene(Body& b, const DepthBody& from, std::span<const Archetype> kinds, const PlatformWorld& p) {
    const graphics::ClipBox* box = strike_box(b, kinds);
    fix32 f;
    if (!scene_fraction(b, from, box != nullptr ? *box : POINT, p, f)) return false;
    b.pos.x = from.x + (b.pos.x - from.x) * f;
    b.pos.z = from.z + (b.pos.z - from.z) * f;
    b.pos.y = from.y + (b.pos.y - from.y) * f;
    return true;
}

} // namespace

EntId spawn_projectile(BodyPool& pool, StageSections* sections, EntId owner, const Shot& shot, const BrawlWorld& w) {
    const Body* from = pool.find(owner);
    if (from == nullptr || shot.life == 0 || shot.kind >= w.kinds.size()) return EntId{};
    const Archetype& a = w.kinds[shot.kind];
    if (a.role != Role::Projectile || a.move_count == 0) return EntId{};
    const SectionBody* section = sections != nullptr ? sections->find(owner) : nullptr;
    Body b;
    b.pos = shot.at;
    b.facing = facing_of(shot.at.vx, from->facing);
    b.team = from->team;
    b.owner = owner;
    b.kind = shot.kind;
    b.clip = a.moves[0].clip;
    b.move = 0;
    b.life = shot.life;
    if (modes(w) != nullptr && own_side(*w.level, from->pos.x, section != nullptr, b.pos.x)) stop(b);
    const EntId id = pool.spawn(b);
    if (id == EntId{} || section == nullptr || sections->add(id, character::Character{})) return id;
    pool.despawn(id);
    return EntId{};
}

void fly_projectile(Body& b, const BrawlWorld& w, bool platform) {
    ++b.age;
    ++b.elapsed;
    if (b.life == 0) return;
    const DepthBody from = b.pos;
    b.pos.x = from.x + from.vx;
    b.pos.z = from.z + from.vz;
    b.pos.y = from.y + from.vy;
    const bool left = clamp_to_mode(b, from, w, platform);
    const bool flat = platform && w.platform != nullptr;
    const bool stopped = flat ? stop_in_scene(b, from, w.kinds, *w.platform) : stop_on_floor(b, from, *w.floor);
    if (stopped || left) stop(b);
}

void retire_projectiles(BodyPool& pool, std::span<const Archetype> kinds, const HitEvents& events) {
    for (uint32_t i = 0; i < events.count; ++i) {
        Body* a = events.at[i].kept ? pool.find(events.at[i].attacker) : nullptr;
        if (a != nullptr && role_of(*a, kinds) == Role::Projectile) a->life = 0;
    }
    for (uint32_t i = 0; i < pool.count; ++i) {
        Body& b = pool.bodies[i];
        if (role_of(b, kinds) == Role::Projectile && b.life > 0) --b.life;
    }
}

void drop_spent(BodyPool& pool, std::span<const Archetype> kinds, StageSections* sections) {
    for (uint32_t i = pool.count; i-- > 0;) {
        const Body& b = pool.bodies[i];
        if (role_of(b, kinds) != Role::Projectile || b.life > 0) continue;
        const EntId id = b.id;
        if (sections != nullptr) sections->drop(id);
        pool.despawn(id);
    }
}

} // namespace framework::brawl
