#include "fighter_bake.hpp"

#include "fighter_chain.hpp"
#include "fighter_fail.hpp"
#include "fighter_format.hpp"
#include "fighter_name.hpp"
#include "fighter_read.hpp"
#include "section_bake.hpp"

namespace framework::brawl {
namespace {

bool carries(const graphics::ClipSrc& clip, uint8_t box) {
    for (const graphics::ClipFrameSrc& f : clip.frames)
        for (const graphics::ClipBoxSrc& b : f.boxes)
            if (b.kind == graphics::BoxKind::Hit && b.index == box) return true;
    return false;
}

bool sheet_listed(const std::string& sheet, std::span<const graphics::ClipSrc> clips) {
    const std::string prefix = sheet + "/";
    for (const graphics::ClipSrc& c : clips)
        if (c.name.compare(0, prefix.size(), prefix) == 0) return true;
    return false;
}

StrikeRow row_of(core::SectionBuilder& b, const FighterSpec& spec, const MoveSpec& m) {
    const Strike& s = m.strike;
    StrikeRow r{};
    r.clip_offset = b.text(sheet_clip(spec.sheet, m.clip));
    r.box = s.box;
    r.type = static_cast<uint8_t>(s.type);
    r.flags = s.hits_down ? STRIKE_HITS_DOWN : 0;
    r.damage = s.damage;
    r.depth_raw = s.depth.raw;
    r.hitstop = s.hitstop;
    r.hitstun = s.hitstun;
    r.knock_x_raw = s.knock_x.raw;
    r.knock_y_raw = s.knock_y.raw;
    return r;
}

} // namespace

bool check_fighter_clips(const FighterSpec& spec, std::span<const graphics::ClipSrc> clips, FighterBakeError& err) {
    if (!sheet_listed(spec.sheet, clips))
        return fighter_fail(err, spec.sheet_line,
                            "no clip of sheet '" + spec.sheet + "' in the clips of this manifest");
    for (const MoveSpec& m : spec.moves) {
        const std::string name = sheet_clip(spec.sheet, m.clip);
        const std::string what = move_label(m);
        const graphics::ClipSrc* found = nullptr;
        for (const graphics::ClipSrc& c : clips)
            if (c.name == name) found = &c;
        if (found == nullptr)
            return fighter_fail(err, m.line, what + ": no clip '" + name + "' in the clips of this manifest");
        if (!carries(*found, m.strike.box))
            return fighter_fail(err, m.line, what + ": no frame of clip '" + name + "' carries this hit box");
    }
    return check_chain_cancels(spec, clips, err);
}

bool bake_fighter(const std::string& name, const std::string& text, std::span<const graphics::ClipSrc> clips,
                  std::vector<uint8_t>& out, FighterBakeError& err) {
    FighterSpec spec;
    if (!parse_fighter(text, spec, err) || !check_fighter_clips(spec, clips, err)) return false;
    core::SectionBuilder b(sizeof(FighterRow));
    std::vector<StrikeRow> strikes;
    for (const MoveSpec& m : spec.moves) strikes.push_back(row_of(b, spec, m));
    std::vector<uint32_t> chain;
    if (!chain_moves(spec, chain, err)) return false;
    FighterRow row{};
    row.name_offset = b.text(name);
    row.sheet_offset = b.text(spec.sheet);
    row.speed_x_raw = spec.speed_x.raw;
    row.speed_z_raw = spec.speed_z.raw;
    row.run_x_raw = spec.run_x.raw;
    row.gravity_raw = spec.gravity.raw;
    row.jump_vy_raw = spec.jump_vy.raw;
    row.depth_raw = spec.depth.raw;
    row.hp = spec.hp;
    row.down_ticks = spec.down;
    row.getup_ticks = spec.getup;
    row.buffer_ticks = spec.buffer;
    row.move_offset = b.block(strikes.data(), strikes.size() * sizeof(StrikeRow), alignof(StrikeRow));
    row.move_count = static_cast<uint32_t>(strikes.size());
    row.chain_offset = b.block(chain.data(), chain.size() * sizeof(uint32_t), alignof(uint32_t));
    row.chain_count = static_cast<uint32_t>(chain.size());
    std::string why;
    if (!b.finish(FIGHTER_MAGIC, FIGHTER_VERSION, 1, &row, out, why)) return fighter_fail(err, 0, why);
    FighterTable probe;
    if (!probe.open(out.data(), out.size())) return fighter_fail(err, 0, "baked fighter table fails its own reader");
    return true;
}

} // namespace framework::brawl
