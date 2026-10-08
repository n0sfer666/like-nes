#pragma once
#include <string>
#include <vector>

#include "archetype.hpp"
#include "brawl_step.hpp"
#include "clip_bake.hpp"
#include "fighter_bake.hpp"
#include "hit_apply.hpp"
#include "hit_collect.hpp"

namespace framework::brawl::test {

inline const char* DUMMY_TEXT = "sheet   | dummy\n"
                                "speed_x | 2\n"
                                "speed_z | 1\n"
                                "run_x   | 3\n"
                                "gravity | 0.5\n"
                                "jump_vy | 6\n"
                                "depth   | 4\n"
                                "hp      | 100\n"
                                "down    | 6\n"
                                "getup   | 4\n"
                                "buffer  | 6\n"
                                "run_tap | 5\n"
                                "dodge   | 5\n"
                                "chain   | jab jab flurry\n"
                                "move | jab | hit0\n"
                                "type | light\ndamage | 5\ndepth | 4\nhitstop | 3\nhitstun | 8\n"
                                "knock_x | 1\nknock_y | 0\nhits_down | no\nslide | no\n"
                                "move | flurry | hit0\n"
                                "type | heavy\ndamage | 7\ndepth | 4\nhitstop | 2\nhitstun | 10\n"
                                "knock_x | 2\nknock_y | 3\nhits_down | no\nslide | no\n"
                                "move | run_jab | hit0\nclip | jab\n"
                                "type | heavy\ndamage | 9\ndepth | 4\nhitstop | 3\nhitstun | 12\n"
                                "knock_x | 3\nknock_y | 2\nhits_down | no\nslide | yes\n";

constexpr graphics::Rect16 HURT{-8, -32, 16, 32};
constexpr graphics::Rect16 REACH{8, -24, 12, 6};

inline graphics::ClipFrameSrc frame(bool hit) {
    graphics::ClipFrameSrc f;
    f.w = 16;
    f.h = 32;
    f.anchor_x = 8;
    f.anchor_y = 32;
    f.boxes.push_back({graphics::BoxKind::Hurt, 0, HURT});
    if (hit) f.boxes.push_back({graphics::BoxKind::Hit, 0, REACH});
    return f;
}

inline graphics::ClipSrc dummy_clip(const char* tag, std::vector<bool> hits, uint16_t flags) {
    graphics::ClipSrc c;
    c.name = std::string("dummy/") + tag;
    c.flags = flags;
    for (const bool h : hits) c.frames.push_back(frame(h));
    return c;
}

inline graphics::ClipSrc cancels_at(graphics::ClipSrc c, std::size_t frame) {
    c.frames[frame].event = CANCEL_EVENT;
    return c;
}

inline std::vector<graphics::ClipSrc> dummy_clips() {
    return {dummy_clip("idle", {false}, graphics::CLIP_LOOP), dummy_clip("walk", {false, false}, graphics::CLIP_LOOP),
            dummy_clip("jump", {false}, graphics::CLIP_ONCE),
            cancels_at(dummy_clip("jab", {false, true, true, false}, graphics::CLIP_ONCE), 1),
            dummy_clip("flurry", {true, false, true, true, false}, graphics::CLIP_LOOP),
            dummy_clip("hurt", {false, false}, graphics::CLIP_LOOP), dummy_clip("fall", {false}, graphics::CLIP_ONCE),
            dummy_clip("down", {false}, graphics::CLIP_ONCE),
            dummy_clip("getup", {false, false, false}, graphics::CLIP_LOOP | graphics::CLIP_PINGPONG),
            dummy_clip("block", {false, false}, graphics::CLIP_ONCE),
            dummy_clip("dodge", {false, false, false}, graphics::CLIP_ONCE)};
}

struct Arena {
    std::vector<uint8_t> clip_bytes, fighter_bytes;
    graphics::ClipTable clips;
    FighterTable fighter;
    Archetype kinds[1];
    uint16_t jab = 0, flurry = 0;
    DepthFloor floor;
    std::string error;

    Arena(const Arena&) = delete;
    Arena() : Arena(dummy_clips()) {}
    explicit Arena(const std::vector<graphics::ClipSrc>& src, const std::string& text = DUMMY_TEXT) {
        FighterBakeError fe;
        if (!graphics::bake_clips(src, clip_bytes, error) ||
            !bake_fighter("dummy.fighter", text, src, fighter_bytes, fe)) {
            error += fe.message;
            return;
        }
        if (!clips.open(clip_bytes.data(), clip_bytes.size()) ||
            !fighter.open(fighter_bytes.data(), fighter_bytes.size()) ||
            !make_archetype(fighter, clips, kinds[0], error) || !clip_index(clips, "dummy/jab", jab) ||
            !clip_index(clips, "dummy/flurry", flurry))
            error += " the arena does not open";
        floor.band = FloorRect{fix32::from_int(-1000), fix32::from_int(-1000), fix32::from_int(1000),
                               fix32::from_int(1000)};
    }

    BrawlWorld world(bool friendly_fire = false) const { return BrawlWorld{kinds, &floor, HitRules{friendly_fire}}; }
};

inline Body dummy(int32_t x, int32_t z, int8_t facing, uint8_t team, const Archetype& a) {
    Body b;
    b.pos.x = fix32::from_int(x);
    b.pos.z = fix32::from_int(z);
    b.facing = facing;
    b.team = team;
    b.hp = 100;
    b.clip = a.idle;
    return b;
}

inline Command strike(const Arena& arena, uint16_t clip) {
    Command c;
    const Archetype& a = arena.kinds[0];
    for (uint32_t i = a.move_count; i-- > 0;)
        if (a.moves[i].clip == clip) c.strike = static_cast<uint16_t>(i);
    return c;
}

inline uint32_t tick(BodyPool& pool, const Arena& arena, std::vector<Command> commands, bool friendly_fire = false) {
    HitEvents events;
    commands.resize(pool.count);
    step_brawl(pool, commands, arena.world(friendly_fire), events);
    return events.count;
}

inline Command walk_right() {
    Command c;
    c.input.present = true;
    c.input.move.move_x = fix32::from_int(1);
    return c;
}

struct Ring {
    Arena arena;
    BodyPool pool;

    Ring() = default;
    explicit Ring(const std::vector<graphics::ClipSrc>& src, const std::string& text = DUMMY_TEXT) : arena(src, text) {}

    Body& put(int32_t x, int8_t facing, uint8_t team) {
        return *pool.find(pool.spawn(dummy(x, 0, facing, team, arena.kinds[0])));
    }
    void play(Body& b, uint16_t clip) const {
        b.clip = clip;
        b.move = strike(arena, clip).strike;
    }
    uint16_t row(const char* name) const {
        uint16_t m = NO_STRIKE;
        return find_named(arena.kinds[0], name, m) ? m : NO_STRIKE;
    }
    void mid_jab(Body& b) const {
        play(b, arena.jab);
        b.elapsed = 1;
    }
    Strike& move(uint16_t clip) {
        Archetype& a = arena.kinds[0];
        uint32_t i = 0;
        while (i + 1 < a.move_count && a.moves[i].clip != clip) ++i;
        return a.moves[i].strike;
    }
    HitEvents collect(bool friendly_fire = false) const {
        HitEvents e;
        collect_hits(pool, arena.kinds, HitRules{friendly_fire}, e);
        return e;
    }
    HitEvents hit() {
        HitEvents e = collect();
        apply_hits(pool, arena.kinds, e);
        return e;
    }
};

} // namespace framework::brawl::test
