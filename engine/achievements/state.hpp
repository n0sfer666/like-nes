#pragma once
#include "def.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ach {

constexpr uint8_t STATE_MAGIC[4] = {'L', 'N', 'A', 'P'};
constexpr uint32_t STATE_VERSION = 1;

// Потолок чтения снимка с диска: файл берётся из внешнего пути, валидация идёт ПОСЛЕ чтения,
// поэтому раздутый файл обязан отбиваться по размеру, а не через OOM.
constexpr std::size_t STATE_MAX_SIZE = 8u * 1024u * 1024u;
constexpr std::size_t STATE_HEADER_SIZE = 24;
constexpr std::size_t STATE_STAT_SIZE = 16;
constexpr std::size_t STATE_ACH_SIZE = 8;

struct StatRecord {
    Id id;
    uint64_t value;
};

struct Snapshot {
    std::vector<StatRecord> stats;
    std::vector<Id> unlocked;
};

enum class DecodeResult : uint32_t {
    Ok = 0,
    TooShort = 1,
    BadMagic = 2,
    BadVersion = 3,
    BadSize = 4,
    BadHash = 5,
    Missing = 6,
    Unreadable = 7,
};

void encode(const Snapshot& snap, std::vector<uint8_t>& out);
DecodeResult decode(const uint8_t* data, std::size_t size, Snapshot& out);
const char* decode_reason(DecodeResult r);

} // namespace ach
