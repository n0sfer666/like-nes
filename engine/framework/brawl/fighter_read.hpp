#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "fighter.hpp"
#include "fighter_format.hpp"
#include "section_open.hpp"

namespace framework::brawl {

class FighterTable {
public:
    bool open(const void* data, std::size_t size);
    bool valid() const { return row_ != nullptr; }

    const char* name() const;
    const char* sheet() const;
    DepthProfile profile() const;
    fix32 run_x() const;
    fix32 depth() const;
    uint32_t hp() const;
    uint32_t down_ticks() const;
    uint32_t getup_ticks() const;
    uint32_t buffer_ticks() const;
    uint32_t run_tap_ticks() const;

    uint32_t move_count() const { return static_cast<uint32_t>(moves_.size()); }
    const char* move_name(uint32_t index) const;
    const char* move_clip(uint32_t index) const;
    bool move(uint32_t index, Strike& out) const;

    uint32_t chain_count() const { return static_cast<uint32_t>(chain_.size()); }
    uint32_t chain_move(uint32_t index) const { return index < chain_.size() ? chain_[index] : 0; }

private:
    core::SectionView view_;
    const FighterRow* row_ = nullptr;
    std::span<const StrikeRow> moves_;
    std::span<const uint32_t> chain_;
};

} // namespace framework::brawl
