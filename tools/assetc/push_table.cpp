#include "bakers.hpp"

#include "baker_guid.hpp"
#include "format.hpp"

namespace asset::bakers {

// Сборка ассета одна на всех: тип, кодек и резидентность у zero-parse таблицы не выбираются, а
// следуют из самого шва. Пекари различаются ИСХОДНИКОМ и именем guid — и только ими.
void push_table(const char* name, std::vector<uint8_t>&& table, std::vector<AssetInput>& out) {
    AssetInput a;
    a.guid = guid_of(name);
    a.type = AssetType::Raw;
    a.codec = Codec::Raw;
    a.residency = Residency::Mmap;
    a.uncompressed_size = static_cast<uint32_t>(table.size());
    a.payload = std::move(table);
    out.push_back(std::move(a));
}

} // namespace asset::bakers
