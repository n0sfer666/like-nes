#include "section_open.hpp"

#include <cstring>

namespace framework::core {

bool SectionView::block(uint32_t offset, uint64_t bytes, std::size_t align) const {
    if (offset < rows_end || offset % align != 0) return false;
    return static_cast<uint64_t>(offset) + bytes <= header->strings_offset;
}

bool open_section(const void* data, std::size_t size, const uint8_t (&magic)[4], uint32_t version,
                  std::size_t row_size, std::size_t align, SectionView& out) {
    out = SectionView{};
    if (data == nullptr || size < sizeof(SectionHeader)) return false;
    // Zero-parse: адрес базы проверяется на выравнивание самой строгой записи секции до первого
    // чтения заголовка как POD (ADR 0003).
    if (reinterpret_cast<std::uintptr_t>(data) % align != 0) return false;
    const auto* base = static_cast<const uint8_t*>(data);
    // Zero-parse: заголовок читается на месте из выровненного буфера секции (ADR 0003).
    const auto* h = reinterpret_cast<const SectionHeader*>(base);
    if (std::memcmp(h->magic, magic, sizeof(h->magic)) != 0) return false;
    if (h->version != version || h->total_size > size) return false;
    if (h->rows_offset != sizeof(SectionHeader)) return false;
    const uint64_t rows_end = h->rows_offset + static_cast<uint64_t>(h->count) * row_size;
    if (rows_end > h->strings_offset || h->strings_offset >= h->total_size) return false;
    if (base[h->total_size - 1] != '\0') return false;
    out.base = base;
    out.header = h;
    out.rows_end = static_cast<uint32_t>(rows_end);
    // Zero-parse: блоб имён — байты секции, читаемые как C-строки (ADR 0003).
    out.strings = reinterpret_cast<const char*>(base + h->strings_offset);
    out.strings_size = h->total_size - h->strings_offset;
    return true;
}

} // namespace framework::core
