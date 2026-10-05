#include "credits_read.hpp"

#include <span>

namespace framework::core {
namespace {

bool filled(const SectionView& v, uint32_t offset) {
    return v.text(offset) && v.strings[offset] != '\0';
}

bool row_ok(const SectionView& v, const CreditRow& r) {
    return filled(v, r.pack_offset) && filled(v, r.author_offset) && filled(v, r.license_offset) &&
           filled(v, r.url_offset) && v.text(r.attribution_offset);
}

} // namespace

bool CreditTable::open(const void* data, std::size_t size) {
    view_ = SectionView{};
    rows_ = nullptr;
    SectionView v;
    if (!open_section(data, size, CREDITS_MAGIC, CREDITS_VERSION, sizeof(CreditRow), alignof(CreditRow), v))
        return false;
    if (v.header->count == 0 || v.rows_end != v.header->strings_offset) return false;
    const std::span<const CreditRow> rows = v.at<CreditRow>(v.header->rows_offset, v.header->count);
    for (const CreditRow& r : rows)
        if (!row_ok(v, r)) return false;
    view_ = v;
    rows_ = rows.data();
    return true;
}

Credit CreditTable::at(uint32_t index) const {
    if (index >= count()) return {};
    const CreditRow& r = rows_[index];
    const char* s = view_.strings;
    return Credit{s + r.pack_offset, s + r.author_offset, s + r.license_offset, s + r.url_offset,
                  s + r.attribution_offset};
}

} // namespace framework::core
