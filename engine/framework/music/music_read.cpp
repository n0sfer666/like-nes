#include "music_read.hpp"

#include <algorithm>
#include <cstring>

#include "hash.hpp"
#include "music_validation.hpp"

namespace framework::music {
uint64_t music_id(std::string_view name) { return asset::fnv1a(name.data(), name.size()); }
bool detail::name_ok(std::string_view name) {
    return !name.empty() && name.size() <= MAX_MUSIC_NAME && std::all_of(name.begin(), name.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    });
}
bool detail::row_ok(const MusicRow& row) {
    return row.frames > 0 && row.rate == MUSIC_SAMPLE_RATE && (row.channels == 1 || row.channels == 2) &&
        row.loop_start < row.loop_end && row.loop_end <= row.frames && row.reserved == 0;
}
bool MusicTable::open(const void* data, size_t size) {
    view_ = {}; rows_ = nullptr;
    core::SectionView view;
    if (size > MAX_MUSIC_BYTES || !core::open_section(data, size, MUSIC_MAGIC, MUSIC_VERSION,
        sizeof(MusicRow), alignof(MusicRow), view)) return false;
    if (view.header->count == 0 || view.header->count > MAX_MUSIC_TRACKS || view.header->total_size != size ||
        view.rows_end != view.header->strings_offset || view.strings[0] != 0) return false;
    const auto rows = view.at<MusicRow>(view.header->rows_offset, view.header->count);
    for (uint32_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (!detail::row_ok(row) || (i && rows[i - 1].id >= row.id) || row.name == 0 || row.name >= view.strings_size ||
            view.strings[row.name - 1] != 0) return false;
        const char* begin = view.strings + row.name;
        const auto* end = static_cast<const char*>(std::memchr(begin, 0, std::min<size_t>(MAX_MUSIC_NAME + 1, view.strings_size - row.name)));
        if (!end) return false;
        const std::string_view name(begin, static_cast<size_t>(end - begin));
        if (!detail::name_ok(name) || row.id != music_id(name)) return false;
    }
    view_ = view; rows_ = rows.data(); return true;
}
const char* MusicTable::name(uint32_t index) const { return index < count() ? view_.strings + rows_[index].name : ""; }
const MusicRow* MusicTable::track(uint32_t index) const { return index < count() ? rows_ + index : nullptr; }
const MusicRow* MusicTable::find(uint64_t id) const {
    if (!rows_) return nullptr;
    const auto* end = rows_ + count();
    const auto* row = std::lower_bound(rows_, end, id, [](const auto& item, uint64_t value) { return item.id < value; });
    return row != end && row->id == id ? row : nullptr;
}
const MusicRow* MusicTable::find(std::string_view name) const {
    if (!detail::name_ok(name)) return nullptr;
    const auto* row = find(music_id(name));
    return row && name == view_.strings + row->name ? row : nullptr;
}
}
