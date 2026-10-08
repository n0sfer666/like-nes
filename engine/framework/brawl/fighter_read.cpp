#include "fighter_read.hpp"

namespace framework::brawl {
namespace {

bool fix_in(int32_t raw, fix32 lo, fix32 hi) { return raw >= lo.raw && raw <= hi.raw; }

bool head_ok(const core::SectionView& v, const FighterRow& r) {
    const fix32 none = fix32::from_int(0);
    const fix32 tiny = fix32::from_raw(1);
    return v.text(r.name_offset) && v.text(r.sheet_offset) && v.strings[r.sheet_offset] != '\0' &&
           fix_in(r.speed_x_raw, none, MAX_FIGHTER_SPEED) && fix_in(r.speed_z_raw, none, MAX_FIGHTER_SPEED) &&
           fix_in(r.run_x_raw, none, MAX_FIGHTER_SPEED) && fix_in(r.gravity_raw, tiny, MAX_FIGHTER_SPEED) &&
           fix_in(r.jump_vy_raw, none, MAX_FIGHTER_SPEED) && fix_in(r.depth_raw, tiny, MAX_FIGHTER_DEPTH) &&
           r.hp >= MIN_HP && r.hp <= MAX_HP && r.down_ticks >= 1 && r.down_ticks <= MAX_HIT_TICKS &&
           r.getup_ticks >= 1 && r.getup_ticks <= MAX_HIT_TICKS && r.buffer_ticks >= 1 &&
           r.buffer_ticks <= MAX_HIT_TICKS && r.run_tap_ticks >= 1 && r.run_tap_ticks <= MAX_HIT_TICKS &&
           r.chain_count >= 1 && r.chain_count <= MAX_CHAIN;
}

bool strike_ok(const core::SectionView& v, const StrikeRow& s) {
    return v.text(s.name_offset) && v.strings[s.name_offset] != '\0' && v.text(s.clip_offset) &&
           v.strings[s.clip_offset] != '\0' && s.box < MAX_HIT_BOXES && s.type <= HIT_TYPE_LAST &&
           (s.flags & ~(STRIKE_HITS_DOWN | STRIKE_SLIDES)) == 0 && s.pad == 0 && s.damage <= MAX_DAMAGE &&
           fix_in(s.depth_raw, fix32::from_raw(1), MAX_FIGHTER_DEPTH) && s.hitstop <= MAX_HIT_TICKS &&
           s.hitstun <= MAX_HIT_TICKS && fix_in(s.knock_x_raw, -MAX_FIGHTER_SPEED, MAX_FIGHTER_SPEED) &&
           fix_in(s.knock_y_raw, -MAX_FIGHTER_SPEED, MAX_FIGHTER_SPEED);
}

} // namespace

bool FighterTable::open(const void* data, std::size_t size) {
    view_ = core::SectionView{};
    row_ = nullptr;
    moves_ = {};
    chain_ = {};
    core::SectionView v;
    if (!core::open_section(data, size, FIGHTER_MAGIC, FIGHTER_VERSION, sizeof(FighterRow), alignof(FighterRow), v))
        return false;
    if (v.header->count != 1) return false;
    const FighterRow& r = v.at<FighterRow>(v.header->rows_offset, 1)[0];
    std::span<const StrikeRow> moves;
    std::span<const uint32_t> chain;
    if (!head_ok(v, r) || !v.take(r.move_offset, r.move_count, moves) || !v.take(r.chain_offset, r.chain_count, chain))
        return false;
    for (const StrikeRow& s : moves)
        if (!strike_ok(v, s)) return false;
    for (const uint32_t m : chain)
        if (m >= moves.size()) return false;
    view_ = v;
    row_ = &r;
    moves_ = moves;
    chain_ = chain;
    return true;
}

const char* FighterTable::name() const { return valid() ? view_.strings + row_->name_offset : ""; }

const char* FighterTable::sheet() const { return valid() ? view_.strings + row_->sheet_offset : ""; }

DepthProfile FighterTable::profile() const {
    if (!valid()) return {};
    return DepthProfile{fix32::from_raw(row_->speed_x_raw), fix32::from_raw(row_->speed_z_raw),
                        fix32::from_raw(row_->gravity_raw), fix32::from_raw(row_->jump_vy_raw)};
}

fix32 FighterTable::run_x() const { return valid() ? fix32::from_raw(row_->run_x_raw) : fix32{}; }

fix32 FighterTable::depth() const { return valid() ? fix32::from_raw(row_->depth_raw) : fix32{}; }

uint32_t FighterTable::hp() const { return valid() ? row_->hp : 0; }

uint32_t FighterTable::down_ticks() const { return valid() ? row_->down_ticks : 0; }

uint32_t FighterTable::getup_ticks() const { return valid() ? row_->getup_ticks : 0; }

uint32_t FighterTable::buffer_ticks() const { return valid() ? row_->buffer_ticks : 0; }

uint32_t FighterTable::run_tap_ticks() const { return valid() ? row_->run_tap_ticks : 0; }

const char* FighterTable::move_name(uint32_t index) const {
    return index < moves_.size() ? view_.strings + moves_[index].name_offset : "";
}

const char* FighterTable::move_clip(uint32_t index) const {
    return index < moves_.size() ? view_.strings + moves_[index].clip_offset : "";
}

bool FighterTable::move(uint32_t index, Strike& out) const {
    if (index >= moves_.size()) return false;
    const StrikeRow& s = moves_[index];
    out.box = s.box;
    out.type = static_cast<HitType>(s.type);
    out.hits_down = (s.flags & STRIKE_HITS_DOWN) != 0;
    out.slides = (s.flags & STRIKE_SLIDES) != 0;
    out.damage = s.damage;
    out.depth = fix32::from_raw(s.depth_raw);
    out.hitstop = s.hitstop;
    out.hitstun = s.hitstun;
    out.knock_x = fix32::from_raw(s.knock_x_raw);
    out.knock_y = fix32::from_raw(s.knock_y_raw);
    return true;
}

} // namespace framework::brawl
