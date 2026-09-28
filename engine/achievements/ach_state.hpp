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
// Смещение хеша внутри заголовка. Именем, а не литералом по месту: писатель, читатель и
// перепечатка фаззера указывают на одно поле, и разойтись им теперь негде. Разъехавшиеся копии
// одного числа — это находка 5 аудита #21 (семья FNV в трёх файлах), повторять её не нужно.
constexpr std::size_t STATE_HASH_OFFSET = 16;
constexpr std::size_t STATE_HASH_SIZE = 8;
// Поле обязано ПОМЕЩАТЬСЯ в заголовок. Три цикла записи/чтения хеша (писатель, читатель и
// перепечатка фаззера) ходят по этим двум числам, и сдвинь кто-нибудь смещение, не тронув размер
// заголовка, — запись ушла бы в полезную нагрузку молча, а гейт 9 доложил бы о вакууме вместо
// настоящей причины. Сборка ловит это раньше любого прогона.
static_assert(STATE_HASH_OFFSET + STATE_HASH_SIZE <= STATE_HEADER_SIZE,
              "the hash field must fit inside the state header");
// И ШИРИНА: поле пишется `put_u64`, читается `get_u64`, а другой ширины эти примитивы не умеют.
// Без этой строки смена размера на 4 прошла бы молча — писатель положил бы четыре байта, читатель
// сравнил бы восемь, и разъехалась бы ровно та семья копий, ради которой имя и заводилось.
static_assert(STATE_HASH_SIZE == sizeof(uint64_t),
              "the hash field is written and read as a single uint64");
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
