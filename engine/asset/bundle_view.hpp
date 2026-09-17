#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "format.hpp"

// Zero-parse вид на mmap'нутый бандл: reinterpret_cast заголовка/таблицы БЕЗ парсинга
// (target-native раскладка). trusted — свои pak'и; validate() — validated-режим для
// внешних/мод-pak'ов (bounds-check всех offset'ов, reject НЕ crash — спека #5 безопасность).
namespace asset {

// Выравнивание БАЗЫ региона. Вид читает заголовок и таблицу приведением, а payload отдаёт
// указателем внутрь того же региона — и выравнивание payload'а проверяется ОТ БАЗЫ. Поэтому база
// обязана быть не слабее самого строгого из трёх требований: заголовка, строки таблицы и
// `PAYLOAD_ALIGN`. База по заголовку (8) пропускала бы регион, у которого каждый payload оказался
// бы невыровненным на 16, — а его читают как uint32* и как SIMD (аудит #21, ревью A·2).
constexpr size_t BASE_ALIGN =
    std::max({alignof(BundleHeader), alignof(AssetEntry), static_cast<size_t>(PAYLOAD_ALIGN)});

// Причина отказа `open()`. Библиотека ассетов не печатает ни строки — ни в stderr, ни в лог, — и
// до сих пор «не открылось» было единственным, что она умела сказать: битый диск, чужая версия
// формата и сдвинутый буфер приезжали вызывающему одним и тем же `false`. Со сверкой штампа
// (аудит #21, A·2·5) различать их стало обязательно: «файл испорчен» — это чинится перекачкой
// мода, а «чужая версия формата» — пересборкой бандла, и совет вызывающего зависит от того, какая
// из двух причин сработала.
//
// Оборванная закачка стоит СВОИМ значением, а не внутри `Malformed` (решение владельца по ревью
// A·2·5): это ровно тот сценарий, ради которого находка и заведена, и в общем «раскладка не
// сошлась» недокачанный мод получал бы совет «пересобери бандл» вместо «перекачай». Причина
// известна ровно там, где проверка падает, — на возврате `bool` она терялась.
enum class OpenResult : uint32_t {
    Ok,
    NoRegion,      // указателя на регион не дали вовсе
    Misaligned,    // база слабее BASE_ALIGN — приведение заголовка было бы UB
    WrongVersion,  // подпись, версия формата, порядок байт, размер заголовка → пересобрать
    Truncated,     // байтов меньше, чем обещает заголовок → перекачать
    Malformed,     // остальная раскладка: границы и выравнивание смещений таблицы и payload'ов
    Corrupted,     // конверт цел, но bundle_hash не сошёлся с байтами
};

class BundleView {
public:
    // base/size — mmap-регион. trusted=false → жёсткая валидация раскладки.
    bool open(const uint8_t* base, size_t size, bool trusted);
    // Почему отказал последний `open()`. Спрашивается ПОСЛЕ него: до первого вызова здесь `Ok`,
    // и это не утверждение «регион открыт» — на него отвечает `valid()`.
    OpenResult open_reason() const { return reason_; }

    const BundleHeader& header() const {
        return *reinterpret_cast<const BundleHeader*>(base_);
    }
    uint32_t count() const { return header().asset_count; }

    const AssetEntry& entry(uint32_t i) const {
        return reinterpret_cast<const AssetEntry*>(base_ + header().table_offset)[i];
    }

    const AssetEntry* find(uint64_t guid) const {
        for (uint32_t i = 0; i < count(); ++i)
            if (entry(i).guid == guid) return &entry(i);
        return nullptr;
    }

    // Zero-copy указатель на payload ассета внутри mmap-региона.
    const uint8_t* payload(const AssetEntry& e) const { return base_ + e.payload_offset; }

    bool valid() const { return base_ != nullptr; }

private:
    OpenResult envelope_reason(size_t size) const;
    OpenResult hash_reason() const;

    const uint8_t* base_ = nullptr;
    size_t size_ = 0;
    OpenResult reason_ = OpenResult::Ok;
};

} // namespace asset
