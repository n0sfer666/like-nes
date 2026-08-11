#pragma once
#include <cstdint>

#include "fixed.hpp"

// Генератор раскладки сцен нагрузки — целочисленный LCG, а не `std::rand` и не `<random>`. Причина
// не в скорости: `std::rand` не обязан давать одну последовательность в разных реализациях
// библиотеки, а распределения из `<random>` не обязаны совпадать между ними даже при одном движке.
// Сцена, которая на macOS и на Windows раскладывается по-разному, сделала бы счётчики работы
// несравнимыми — то есть ровно то, ради чего они и заведены вместо стенных часов.
//
// Файл отдельный от сцен, потому что ответственность отдельная: у генератора собственный контракт
// (верхние биты, воспроизводимость между libstdc++, libc++ и MSVC), и проверяется он не тем, чем
// проверяется раскладка.
namespace framework::physics::load {

class Lcg {
public:
    explicit Lcg(uint32_t seed) : state_(seed) {}
    uint32_t next() {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }
    // Целое из [0, span). Верхние биты, а не остаток: младшие у LCG заметно менее случайны, и
    // раскладка по остатку ложится полосами — куча получилась бы решёткой, а не кучей.
    uint32_t below(uint32_t span) {
        return static_cast<uint32_t>((static_cast<uint64_t>(next() >> 8) * span) >> 24);
    }
    fix32 spread(int32_t half) {
        return fix32::from_int(static_cast<int32_t>(below(static_cast<uint32_t>(2 * half))) - half);
    }

private:
    uint32_t state_;
};

} // namespace framework::physics::load
