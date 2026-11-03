#pragma once
#include <string_view>

#include "music_format.hpp"
#include "section_open.hpp"

namespace framework::music {
uint64_t music_id(std::string_view name);
// Zero-copy/zero-heap. Выравненный immutable buffer живёт дольше reader/views.
class MusicTable {
public:
    bool open(const void* data, size_t size);
    uint32_t count() const { return view_.header ? view_.header->count : 0; }
    const char* name(uint32_t index) const;
    const MusicRow* track(uint32_t index) const;
    const MusicRow* find(uint64_t id) const;
    const MusicRow* find(std::string_view name) const;
private:
    core::SectionView view_;
    const MusicRow* rows_ = nullptr;
};
}
