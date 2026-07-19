#include "bundle_writer.hpp"

#include <algorithm>
#include <cstring>

#include "hash.hpp"

namespace asset {

namespace {

// Запись POD в буфер по смещению (target-native, little-endian desktop).
template <typename T>
void put(std::vector<uint8_t>& buf, uint32_t off, const T& v) {
    std::memcpy(buf.data() + off, &v, sizeof(T));
}

} // namespace

std::vector<uint8_t> write_bundle(std::vector<AssetInput> assets) {
    // Детерминизм #1: фиксированный порядок ассетов (сорт по стабильному guid).
    std::sort(assets.begin(), assets.end(),
              [](const AssetInput& a, const AssetInput& b) { return a.guid < b.guid; });

    const uint32_t header_size = sizeof(BundleHeader);
    const uint32_t table_offset = header_size;
    const uint32_t table_size = static_cast<uint32_t>(assets.size()) * sizeof(AssetEntry);

    // Раскладка payload'ов — по порядку таблицы, каждый выровнен на PAYLOAD_ALIGN.
    // Смещения/размеры — uint32 в формате: guard от тихого усечения (>4GB) → пустой вывод = ошибка.
    auto align64 = [](uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); };
    uint64_t cursor = align64(table_offset + table_size, PAYLOAD_ALIGN);
    std::vector<uint32_t> offsets(assets.size());
    for (size_t i = 0; i < assets.size(); ++i) {
        if (assets[i].payload.size() > UINT32_MAX) return {};
        offsets[i] = static_cast<uint32_t>(cursor);
        cursor = align64(cursor + assets[i].payload.size(), PAYLOAD_ALIGN);
        if (cursor > UINT32_MAX) return {}; // бандл не влезает в uint32-адресацию
    }
    const uint32_t total = static_cast<uint32_t>(cursor);

    std::vector<uint8_t> buf(total, 0); // паддинг детерминирован (нули)

    BundleHeader h{};
    std::memcpy(h.magic, MAGIC, 4);
    h.fmt_version = FMT_VERSION;
    h.endian = ENDIAN_LE;
    h.header_size = header_size;
    h.asset_count = static_cast<uint32_t>(assets.size());
    h.table_offset = table_offset;
    h.total_size = total;
    h.bundle_hash = 0;
    put(buf, 0, h);

    for (size_t i = 0; i < assets.size(); ++i) {
        const AssetInput& a = assets[i];
        AssetEntry e{};
        e.guid = a.guid;
        e.content_hash = fnv1a(a.payload.data(), a.payload.size());
        e.type = static_cast<uint32_t>(a.type);
        e.codec = static_cast<uint32_t>(a.codec);
        e.residency = static_cast<uint32_t>(a.residency);
        e.payload_offset = offsets[i];
        e.payload_size = static_cast<uint32_t>(a.payload.size());
        e.uncompressed_size = a.uncompressed_size;
        e.tex_w = a.tex_w;
        e.tex_h = a.tex_h;
        e.tex_format = a.tex_format;
        e.variant_key = a.variant_key;
        put(buf, table_offset + static_cast<uint32_t>(i) * sizeof(AssetEntry), e);
        std::memcpy(buf.data() + offsets[i], a.payload.data(), a.payload.size());
    }

    // bundle_hash считается по всем байтам с обнулённым полем (уже 0) → детерминирован.
    const uint64_t bh = fnv1a(buf.data(), buf.size());
    put(buf, offsetof(BundleHeader, bundle_hash), bh);

    return buf;
}

} // namespace asset
