#pragma once
#include <cstdint>
#include <vector>

// Транскод KTX2/UASTC → BC7 (basis transcoder). Спека #5 «текстуры → KTX2/basis → BC7/ASTC».
// CPU-работа на стороне GPU-consumer'а (вне sim-потока). Детерминирована (фикс. формат).
namespace asset {

// bc7 — плотный BC7 (16 байт/4x4-блок). Возврат false при невалидном KTX2.
bool ktx2_to_bc7(const uint8_t* ktx2, uint32_t size, std::vector<uint8_t>& bc7,
                 uint32_t& w, uint32_t& h);

} // namespace asset
