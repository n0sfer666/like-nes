#include "save_format.hpp"

#include <array>
#include <cstring>

#include "save_wire.hpp"

namespace save {
namespace {
constexpr uint16_t CONTAINER_VERSION = 1;
constexpr uint8_t MAGIC[]{'L', 'N', 'S', 'V'};

// IEEE CRC32, reflected. Таблица строится при компиляции; поля wire читаются только побайтно.
constexpr auto crc_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t n = 0; n < table.size(); ++n) {
        uint32_t crc = n;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0);
        table[n] = crc;
    }
    return table;
}
constexpr auto CRC = crc_table();

uint32_t checksum(std::span<const uint8_t> bytes) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i >= 20 && i < RECORD_HEADER_SIZE) continue;
        crc = (crc >> 8) ^ CRC[(crc ^ bytes[i]) & 255u];
    }
    return crc ^ 0xffffffffu;
}

void put(uint8_t* dest, uint64_t value, int size) {
    for (int i = 0; i < size; ++i) dest[i] = static_cast<uint8_t>(value >> (8 * i));
}
uint64_t get(const uint8_t* source, int size) {
    uint64_t value = 0;
    for (int i = 0; i < size; ++i) value |= static_cast<uint64_t>(source[i]) << (8 * i);
    return value;
}
}

void detail::reseal_record(std::span<uint8_t> bytes) {
    if (bytes.size() >= RECORD_HEADER_SIZE) put(bytes.data() + 20, checksum(bytes), 4);
}

bool encode_record(uint16_t version, uint64_t writes, std::span<const uint8_t> body, std::vector<uint8_t>& out) {
    if (version == 0 || body.size() > MAX_BODY_SIZE) return false;
    // body может заимствовать out: строим новый образ до изменения старого буфера.
    std::vector<uint8_t> bytes(RECORD_HEADER_SIZE + body.size());
    std::memcpy(bytes.data(), MAGIC, sizeof MAGIC);
    put(bytes.data() + 4, CONTAINER_VERSION, 2);
    put(bytes.data() + 6, version, 2);
    put(bytes.data() + 8, body.size(), 4);
    put(bytes.data() + 12, writes, 8);
    if (!body.empty()) std::memcpy(bytes.data() + RECORD_HEADER_SIZE, body.data(), body.size());
    detail::reseal_record(bytes);
    out.swap(bytes);
    return true;
}

RecordResult decode_record(std::span<const uint8_t> bytes, RecordView& out) {
    out = {};
    if (bytes.size() < RECORD_HEADER_SIZE || bytes.size() - RECORD_HEADER_SIZE > MAX_BODY_SIZE ||
        std::memcmp(bytes.data(), MAGIC, sizeof MAGIC) != 0) return RecordResult::Corrupt;
    if (get(bytes.data() + 8, 4) != bytes.size() - RECORD_HEADER_SIZE ||
        get(bytes.data() + 20, 4) != checksum(bytes)) return RecordResult::Corrupt;
    const auto container = static_cast<uint16_t>(get(bytes.data() + 4, 2));
    const auto version = static_cast<uint16_t>(get(bytes.data() + 6, 2));
    if (container == 0 || version == 0) return RecordResult::Corrupt;
    out = {version, get(bytes.data() + 12, 8), bytes.subspan(RECORD_HEADER_SIZE)};
    return container == CONTAINER_VERSION ? RecordResult::Ok : RecordResult::FutureContainer;
}
} // namespace save
