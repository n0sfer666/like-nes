#pragma once
#include <cstddef>
#include <cstdint>

// FNV-1a 64 — детерминированный content-hash (тот же паттерн, что sim golden-hash
// determinism_test.cpp). Байт-ориентированный → не зависит от endianness машины.
namespace asset {

constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
constexpr uint64_t FNV_PRIME = 1099511628211ull;

inline uint64_t fnv1a(const void* data, size_t len, uint64_t h = FNV_OFFSET) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h;
}

// Три формы сверх байтовой — не «удобства», а ровно те рукописные циклы, что до аудита #21 лежали
// по дереву восемью копиями с ТЕМИ ЖЕ константами: FNV по строке (`ach::hash_key`), по восьми
// байтам uint64 (`ach::fnv_u64`, `ide::ipc::fnv_mix`) и по четырём байтам uint32 (`sim_hash_i32`,
// `determinism_test::hash_i32`). Дерево само записало правило в hash_mix.hpp: примитив выносится,
// как только потребителей стало двое, потому что два голдена, сложенные РАЗНЫМИ смешиваниями,
// невозможно сравнивать между собой, а расхождение констант ничем себя не проявляет.
//
// Целое смешивается байтами в ФИКСИРОВАННОМ little-endian порядке через сдвиги: `h ^= v` целым
// словом зависело бы от порядка байт в машине, а голден сверяется между x86 и ARM — там он обязан
// совпасть не потому, что обе платформы сегодня little-endian, а потому, что вопрос не задаётся.
//
// constexpr, потому что две сведённые копии считаются на этапе компиляции (`ach::hash_key` даёт
// `ach::Id`, `ide::ipc::mirror_schema_hash` — хеш раскладки shmem). Байтовая форма выше constexpr
// быть не может: `const void*` в константном выражении не разыменовать.
//
// Вторая семья констант живёт в engine/framework/physics/hash_mix.hpp (каноническая база
// 0xcbf29ce484222325 против нашей 1469598103934665603). Слить их — значит перештамповать голдены
// обеих; это отдельное решение владельца (находка 5 аудита #21), а не побочный эффект выноса.
constexpr uint64_t fnv1a_u32(uint64_t h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (v >> (i * 8)) & 0xffu;
        h *= FNV_PRIME;
    }
    return h;
}

constexpr uint64_t fnv1a_u64(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (i * 8)) & 0xffu;
        h *= FNV_PRIME;
    }
    return h;
}

constexpr uint64_t fnv1a_str(const char* s, uint64_t h = FNV_OFFSET) {
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= FNV_PRIME;
    }
    return h;
}

// Словесная форма: xor кормится ЦЕЛЫМ словом разом, а не байтами. Порядка байт она не фиксирует и
// endian-независимой не является — берётся только там, где голден уже стоит на ней (`sim_hash`
// игры-образца), новому хешу берётся `fnv1a_u64`. Заведена потому, что без неё единственный её
// потребитель писал те же две строки руками именованными константами: копия, которую гейт шва
// (`scripts/check_hash_seam.py`) отбивает наравне с голым литералом, а зеркало в hash_mix.hpp
// (`mix_word`) у второй семьи есть с самого выноса.
constexpr uint64_t fnv1a_word(uint64_t h, uint64_t v) { return (h ^ v) * FNV_PRIME; }

} // namespace asset
