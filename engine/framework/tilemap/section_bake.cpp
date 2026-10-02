#include "section_bake.hpp"

#include <cstring>

#include "section_format.hpp"

namespace framework::tilemap {

SectionBuilder::SectionBuilder(std::size_t rows_bytes)
    : rows_bytes_(rows_bytes), bytes_(sizeof(SectionHeader) + rows_bytes, 0) {}

uint32_t SectionBuilder::block(const void* data, std::size_t bytes, std::size_t align) {
    bytes_.resize((bytes_.size() + align - 1) / align * align, 0);
    const auto offset = static_cast<uint32_t>(bytes_.size());
    const auto* p = static_cast<const uint8_t*>(data);
    if (bytes != 0) bytes_.insert(bytes_.end(), p, p + bytes);
    return offset;
}

uint32_t SectionBuilder::text(const std::string& s) {
    const auto [it, fresh] = seen_.emplace(s, static_cast<uint32_t>(blob_.size()));
    if (fresh) {
        blob_.insert(blob_.end(), s.begin(), s.end());
        blob_.push_back('\0');
    }
    return it->second;
}

bool SectionBuilder::finish(const uint8_t (&magic)[4], uint32_t version, uint32_t count,
                            const void* rows, std::vector<uint8_t>& out, std::string& error) {
    const uint64_t strings = bytes_.size();
    const uint64_t total = strings + blob_.size();
    if (total > 0xFFFFFFFFull) {
        error = "the table does not fit the 32-bit offsets of the format";
        return false;
    }
    SectionHeader h{};
    std::memcpy(h.magic, magic, sizeof(h.magic));
    h.version = version;
    h.count = count;
    h.rows_offset = sizeof(SectionHeader);
    h.strings_offset = static_cast<uint32_t>(strings);
    h.total_size = static_cast<uint32_t>(total);
    std::memcpy(bytes_.data(), &h, sizeof(h));
    if (rows_bytes_ != 0) std::memcpy(bytes_.data() + sizeof(h), rows, rows_bytes_);
    out = bytes_;
    out.insert(out.end(), blob_.begin(), blob_.end());
    return true;
}

} // namespace framework::tilemap
