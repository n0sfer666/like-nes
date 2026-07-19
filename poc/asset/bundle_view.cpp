#include "bundle_view.hpp"

#include <cstring>

#include "hash.hpp"

namespace asset {

bool BundleView::bounds_ok(size_t size) const {
    if (size < sizeof(BundleHeader)) return false;
    BundleHeader h;
    std::memcpy(&h, base_, sizeof(h)); // выровненная копия до доверия раскладке
    if (std::memcmp(h.magic, MAGIC, 4) != 0) return false;
    if (h.fmt_version != FMT_VERSION) return false; // fail-hard → rebake
    if (h.endian != ENDIAN_LE) return false;
    if (h.header_size != sizeof(BundleHeader)) return false;
    if (h.total_size > size) return false;
    // Таблица целиком в границах И выровнена (иначе reinterpret_cast → SIGBUS на strict-align).
    if (h.table_offset % alignof(AssetEntry) != 0) return false;
    uint64_t table_end = static_cast<uint64_t>(h.table_offset) +
                         static_cast<uint64_t>(h.asset_count) * sizeof(AssetEntry);
    if (h.table_offset < sizeof(BundleHeader) || table_end > h.total_size) return false;
    // Каждый payload целиком в границах И выровнен (SPIR-V читается как uint32*).
    const AssetEntry* tbl = reinterpret_cast<const AssetEntry*>(base_ + h.table_offset);
    for (uint32_t i = 0; i < h.asset_count; ++i) {
        if (tbl[i].payload_offset % PAYLOAD_ALIGN != 0) return false;
        uint64_t pe = static_cast<uint64_t>(tbl[i].payload_offset) + tbl[i].payload_size;
        if (tbl[i].payload_offset < table_end || pe > h.total_size) return false;
    }
    return true;
}

bool BundleView::open(const uint8_t* base, size_t size, bool trusted) {
    base_ = nullptr;
    if (!base) return false;
    base_ = base;
    size_ = size;
    if (!trusted && !bounds_ok(size)) {
        base_ = nullptr; // reject, НЕ crash
        return false;
    }
    // trusted всё равно проверяет magic/версию (bit-rot / чужая версия).
    if (trusted) {
        const BundleHeader& h = header();
        if (std::memcmp(h.magic, MAGIC, 4) != 0 || h.fmt_version != FMT_VERSION) {
            base_ = nullptr;
            return false;
        }
    }
    return true;
}

} // namespace asset
