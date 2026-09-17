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

class BundleView {
public:
    // base/size — mmap-регион. trusted=false → жёсткая валидация раскладки.
    bool open(const uint8_t* base, size_t size, bool trusted);

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
    bool bounds_ok(size_t size) const;

    const uint8_t* base_ = nullptr;
    size_t size_ = 0;
};

} // namespace asset
