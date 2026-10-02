#pragma once
#include <cstddef>
#include <cstdint>

#include "bundle_view.hpp"

// Типизированный поиск в открытом бандле (спека #24, В5б): запись по guid, проверенная на ВИД, а не
// только на наличие. Чужой кодек под знакомым guid — отказ с причиной, а не байты, прочитанные как
// пиксели.
namespace asset {

enum class LookupFault : uint8_t { Ok, Missing, WrongType, WrongCodec, WrongFormat, WrongSize };

const char* lookup_fault_name(LookupFault f);

struct RgbaView {
    const uint8_t* pixels = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

// Текстура `pixel` манифеста: Texture, кодек Raw, RGBA8Unorm, ровно w*h*4 байт.
LookupFault raw_rgba8(const BundleView& bundle, uint64_t guid, RgbaView& out);

// Таблица `push_table` (секции LNTM/LNVL/LNOB): Raw, кодек Raw.
LookupFault raw_table(const BundleView& bundle, uint64_t guid, const uint8_t*& data, size_t& size);

} // namespace asset
