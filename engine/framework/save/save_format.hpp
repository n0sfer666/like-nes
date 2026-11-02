#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace save {

inline constexpr size_t RECORD_HEADER_SIZE = 24;
inline constexpr size_t MAX_BODY_SIZE = 16 * 1024 * 1024;
enum class RecordResult { Ok, FutureContainer, Corrupt };

// Не владеет памятью: body заимствует проверенные байты encoded до конца их жизни.
struct RecordView {
    uint16_t version = 0;
    uint64_t writes = 0;
    std::span<const uint8_t> body;
};

// Контейнер LNSV v1; отказ не меняет out. Нулевая версия слота недопустима.
bool encode_record(uint16_t version, uint64_t writes, std::span<const uint8_t> body, std::vector<uint8_t>& out);
// Проверяет framing и CRC до классификации версии; не выделяет память, сбрасывает out при Corrupt.
RecordResult decode_record(std::span<const uint8_t> encoded, RecordView& out);

} // namespace save
